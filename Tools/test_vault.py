#!/usr/bin/env python3
"""Tests of the encrypted vault against independent code (hashlib.pbkdf2_hmac, hmac): python3 -I Tools/test_vault.py"""
import base64, hashlib, hmac, os, random, shutil, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
G = os.path.join(here, '..', 'Software', 'Apps', 'TouchGFX-GUI', 'gui')
work = os.environ.get('TMPDIR', '/tmp') + '/ta_vault_test'
shutil.rmtree(work, ignore_errors=True); os.makedirs(work)
exe = work + '/vault_driver'
subprocess.run(['g++', '-std=c++17', '-O2', '-w', '-I', G + '/include', '-I', here, here + '/vault_driver.cpp', G + '/src/ta/Account.cpp', G + '/src/ta/Sha.cpp', G + '/src/ta/Vault.cpp', '-o', exe], check=True)
fails = 0
def check(c, m):
    global fails
    print(('  ok    ' if c else '  FAIL  ') + m)
    if not c: fails += 1
def run(inp): return subprocess.run([exe], input=inp.encode('latin-1'), capture_output=True).stdout.decode('latin-1').splitlines()
b32 = lambda b: base64.b32encode(b).decode()
S = [bytes(range(i, i + 20)) for i in (1, 40, 90)]
text = '\n'.join(f'otpauth://totp/Svc{i}:user{i}?secret={b32(s)}&issuer=Svc{i}&digits={6 + i % 3}&period={30 * (1 + i % 2)}' for i, s in enumerate(S))
want = [f'Svc{i}|user{i}|0|{6 + i % 3}|{30 * (1 + i % 2)}|{s.hex()}' for i, s in enumerate(S)]

def pydecrypt(path, pin):
    d = open(path, 'rb').read()
    assert d[:4] == b'TAV1'
    it = struct.unpack('<I', d[4:8])[0]; salt = d[8:24]; nonce = d[24:40]; ln = struct.unpack('<H', d[40:42])[0]
    assert len(d) == 42 + ln + 32
    master = hashlib.pbkdf2_hmac('sha256', pin.encode(), salt, it, 32)
    enc = hmac.new(master, b'ta-enc', hashlib.sha256).digest(); mac = hmac.new(master, b'ta-mac', hashlib.sha256).digest()
    tag_ok = hmac.compare_digest(hmac.new(mac, d[:42 + ln], hashlib.sha256).digest(), d[42 + ln:])
    ct = d[42:42 + ln]
    ks = b''.join(hmac.new(enc, nonce + struct.pack('>I', i), hashlib.sha256).digest() for i in range((ln + 31) // 32))
    return it, tag_ok, bytes(a ^ b for a, b in zip(ct, ks))
def pyparse(pt):
    assert pt[:4] == b'TAv1'
    n = pt[4]; o = 5; out = []
    for _ in range(n):
        il = pt[o]; o += 1; iss = pt[o:o + il].decode('latin-1'); o += il
        nl = pt[o]; o += 1; nm = pt[o:o + nl].decode('latin-1'); o += nl
        alg, dg, per, sl = pt[o:o + 4]; o += 4
        out.append(f'{iss}|{nm}|{alg}|{dg}|{per}|{pt[o:o + sl].hex()}'); o += sl
    assert o == len(pt)
    return out

d1 = work + '/a'; os.makedirs(d1)
o = run(f'NEW {d1} 4711 {text.encode().hex()}\n')
check(o == ['SAVED 3'], 'a vault is made and saved')
it, ok, pt = pydecrypt(d1 + '/vault.dat', '4711')
check(it == 20000 and ok, 'Python: 20000 iterations, and the tag is right (HMAC-SHA256 over the file) with PBKDF2 of hashlib')
check(pyparse(pt) == want, 'Python decrypts the vault with the PIN and reads the same three accounts')
o = run(f'UNLOCK {d1} 4711\n')
check(o[:1] == ['OK 3'] and o[1:] == want, 'the app unlocks it with the right PIN')
o = run(f'UNLOCK {d1} 4712\nUNLOCK {d1} 471\nUNLOCK {d1} 47110\n')
check(o == ['WRONG', 'WRONG', 'WRONG'], 'a wrong PIN (also one digit more or less) is refused')

# interop the other way: Python writes a vault, the app reads it
d2 = work + '/b'; os.makedirs(d2)
def pywrite(path, pin, accounts, it=1000):
    pt = b'TAv1' + bytes([len(accounts)])
    for iss, nm, alg, dg, per, sec in accounts:
        pt += bytes([len(iss)]) + iss.encode('latin-1') + bytes([len(nm)]) + nm.encode('latin-1') + bytes([alg, dg, per, len(sec)]) + sec
    salt = os.urandom(16); nonce = os.urandom(16)
    master = hashlib.pbkdf2_hmac('sha256', pin.encode(), salt, it, 32)
    enc = hmac.new(master, b'ta-enc', hashlib.sha256).digest(); mac = hmac.new(master, b'ta-mac', hashlib.sha256).digest()
    ks = b''.join(hmac.new(enc, nonce + struct.pack('>I', i), hashlib.sha256).digest() for i in range((len(pt) + 31) // 32))
    ct = bytes(a ^ b for a, b in zip(pt, ks))
    hdr = b'TAV1' + struct.pack('<I', it) + salt + nonce + struct.pack('<H', len(ct))
    open(path, 'wb').write(hdr + ct + hmac.new(mac, hdr + ct, hashlib.sha256).digest())
pywrite(d2 + '/vault.dat', '90210', [('Py', 'one', 1, 8, 60, bytes(range(30))), ('Zwei \xe4', 'x', 2, 6, 30, bytes(range(64)))], it=777)
o = run(f'UNLOCK {d2} 90210\n')
check(o == ['OK 2', f'Py|one|1|8|60|{bytes(range(30)).hex()}', f'Zwei \xe4|x|2|6|30|{bytes(range(64)).hex()}'], 'a vault that Python wrote (other iterations, SHA-512 and 64-byte keys) is read')

# a damaged file
raw = bytearray(open(d1 + '/vault.dat', 'rb').read())
d3 = work + '/c'; os.makedirs(d3)
res = []
for pos in (5, 12, 30, 45, len(raw) - 1):
    bad = bytearray(raw); bad[pos] ^= 1
    open(d3 + '/vault.dat', 'wb').write(bad)
    res.append(run(f'UNLOCK {d3} 4711\n')[0])
check(all(r in ('WRONG', 'FAIL') for r in res), 'a changed byte anywhere (iterations, salt, text, tag) keeps the vault shut: ' + ', '.join(res))
open(d3 + '/vault.dat', 'wb').write(raw[:len(raw) - 10])
check(run(f'UNLOCK {d3} 4711\n') == ['FAIL'], 'a file that is too short is refused')

# the same accounts, saved twice: not the same bytes (new nonce); a PIN change makes a new key
run(f'NEW {d1} 4711 {text.encode().hex()}\n')
raw2 = open(d1 + '/vault.dat', 'rb').read()
check(raw2 != bytes(raw) and raw2[8:24] != bytes(raw[8:24]) and raw2[24:40] != bytes(raw[24:40]), 'every new vault has its own salt and nonce')
o = run(f'CHANGE {d1} 4711 2468\n')
check(o == ['CHANGED'], 'the PIN is changed')
check(run(f'UNLOCK {d1} 4711\n') == ['WRONG'] and run(f'UNLOCK {d1} 2468\n')[:1] == ['OK 3'], 'the old PIN is refused, the new one opens it')
it, ok, pt = pydecrypt(d1 + '/vault.dat', '2468')
check(ok and pyparse(pt) == want, 'Python reads the vault with the new PIN too')
check(b32(S[0]).encode() not in open(d1 + '/vault.dat', 'rb').read() and S[0] not in open(d1 + '/vault.dat', 'rb').read(), 'the key is nowhere in the file in the clear')
print('all checks passed' if not fails else 'SOME CHECKS FAILED')
sys.exit(1 if fails else 0)
