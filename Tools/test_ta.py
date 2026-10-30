#!/usr/bin/env python3
"""Tests of the codes and of the readers of the keys against independent code (Python's hmac / base64 / json):
python3 -I Tools/test_ta.py   (builds Tools/ta_driver.cpp)"""
import base64, hashlib, hmac, json, os, random, struct, subprocess, sys, urllib.parse
here = os.path.dirname(os.path.abspath(__file__))
G = os.path.join(here, '..', 'Software', 'Apps', 'TouchGFX-GUI', 'gui')
work = os.environ.get('TMPDIR', '/tmp') + '/ta_test'
os.makedirs(work, exist_ok=True)
exe = work + '/ta_driver'
subprocess.run(['g++', '-std=c++17', '-O1', '-w', '-I', G + '/include', here + '/ta_driver.cpp', G + '/src/ta/Account.cpp', G + '/src/ta/Sha.cpp', '-o', exe], check=True)

fails = 0
def check(cond, msg):
    global fails
    print(('  ok    ' if cond else '  FAIL  ') + msg)
    if not cond: fails += 1

def run(inp):
    return subprocess.run([exe], input=inp.encode('latin-1'), capture_output=True).stdout.decode('latin-1').splitlines()

H = {'1': hashlib.sha1, '256': hashlib.sha256, '512': hashlib.sha512}
def totp_py(alg, secret, t, digits, period):
    mac = hmac.new(secret, struct.pack('>Q', t // period), H[alg]).digest()
    o = mac[-1] & 15
    return str(((struct.unpack('>I', mac[o:o + 4])[0] & 0x7fffffff) % 10 ** digits)).zfill(digits)
b32 = lambda b: base64.b32encode(b).decode()
hx = lambda s: s.encode().hex() if s else '-'

print('the codes: RFC 6238 appendix B (8 digits)')
rfc = {'1': (b'12345678901234567890', [94287082, 7081804, 14050471, 89005924, 69279037, 65353130]),
       '256': (b'12345678901234567890123456789012', [46119246, 68084774, 67062674, 91819424, 90698825, 77737706]),
       '512': (b'1234567890123456789012345678901234567890123456789012345678901234', [90693936, 25091201, 99943326, 93441116, 38618901, 47863826])}
times = [59, 1111111109, 1111111111, 1234567890, 2000000000, 20000000000]
lines, want = [], []
for alg, (sec, vals) in rfc.items():
    for t, v in zip(times, vals):
        lines.append(f'T {alg} 8 30 {t} {b32(sec)}'); want.append(str(v).zfill(8))
out = run('\n'.join(lines) + '\n')
check([o.split()[0] for o in out] == want, 'all 18 vectors of RFC 6238 (SHA-1, SHA-256, SHA-512)')

print('the codes: random keys, times, digits, periods')
random.seed(11)
lines, want = [], []
for _ in range(600):
    alg = random.choice(['1', '256', '512'])
    sec = bytes(random.randrange(256) for _ in range(random.choice([10, 16, 20, 32, 48, 64])))
    t = random.randrange(0, 4_000_000_000); d = random.choice([6, 7, 8]); p = random.choice([15, 30, 60, 90, 120])
    lines.append(f'T {alg} {d} {p} {t} {b32(sec)}')
    want.append(f'{totp_py(alg, sec, t, d, p)} {p - t % p}')
out = run('\n'.join(lines) + '\n')
check(out == want, '600 random cases: the code and the seconds left equal Python')

print('the readers')
def acc(issuer, name, alg, digits, period, secret):
    return f'{issuer}|{name}|{alg}|{digits}|{period}|{secret.hex()}'
S1 = bytes(range(1, 21)); S2 = bytes(range(50, 82)); S3 = bytes(range(100, 164))
def parse(text):
    out = run(f'P {hx(text)}\n')
    n, skipped = map(int, out[0].split())
    return n, skipped, out[1:1 + n]

# otpauth lines
uri1 = f'otpauth://totp/GitHub:me%40example.com?secret={b32(S1)}&issuer=GitHub'
uri2 = f'otpauth://totp/Mail%20Co%3Aanna?secret={b32(S2).lower()}&algorithm=SHA256&digits=8&period=60&issuer=Mail%20Co'
uri3 = f'otpauth://totp/plain-name?secret={b32(S3)}&algorithm=SHA512'
hotp = f'otpauth://hotp/Counter:x?secret={b32(S1)}&counter=3'
n, sk, got = parse('\n'.join([uri1, uri2, uri3, hotp, '# a comment', '']) + '\n')
check(n == 3 and sk == 1 and got == [acc('GitHub', 'me@example.com', 0, 6, 30, S1), acc('Mail Co', 'anna', 1, 8, 60, S2), acc('', 'plain-name', 2, 6, 30, S3)],
      'otpauth lines: label, issuer, algorithm, digits, period; hotp skipped')
n, sk, got = parse(uri1 + '\r\n')
check(n == 1 and got == [acc('GitHub', 'me@example.com', 0, 6, 30, S1)], 'a line that ends with CR LF')
n, sk, got = parse('otpauth://totp/Bad?secret=NOT_BASE32!&issuer=X\notpauth://totp/Short?secret=ABCD&issuer=X\notpauth://totp/Steam?secret=' + b32(S1) + '&digits=5\n')
check(n == 0 and sk == 3, 'bad base32, a key that is too short and 5 digits are refused')
n, sk, got = parse('otpauth://totp/' + urllib.parse.quote('Café:jürgen') + '?secret=' + b32(S1))
check(n == 1 and got == [acc('Caf\u00e9', 'j\u00fcrgen', 0, 6, 30, S1)], 'accents in the label come out as ISO 8859-1 (the font has them)')
n, sk, got = parse('otpauth://totp/' + urllib.parse.quote('Ωmega:x') + '?secret=' + b32(S1))
check(n == 1 and got[0].startswith('?mega|x|'), 'a letter that the font does not have becomes ?')

# plain lines
n, sk, got = parse(f'GitHub me {b32(S1)}\n{b32(S2)}\nnonsense\n')
check(n == 2 and sk == 1 and got[0] == acc('', 'GitHub me', 0, 6, 30, S1) and got[1] == acc('', 'Account', 0, 6, 30, S2), 'lines with a name and a key, or a key alone')
n, sk, got = parse(f'My Bank, {" ".join(b32(S1)[i:i+4] for i in range(0, len(b32(S1)), 4))}\n')
check(n == 1 and got[0] == acc('', 'My Bank', 0, 6, 30, S1), 'a key in groups of four letters')

# Google Authenticator export
def varint(v):
    b = bytearray()
    while True:
        x = v & 0x7f; v >>= 7
        if v: b.append(x | 0x80)
        else: b.append(x); break
    return bytes(b)
def ld(field, data): return varint(field << 3 | 2) + varint(len(data)) + data
def vi(field, v): return varint(field << 3) + varint(v)
def otp(secret, name, issuer, algo, digits, typ): return ld(1, secret) + ld(2, name.encode()) + ld(3, issuer.encode()) + vi(4, algo) + vi(5, digits) + vi(6, typ)
payload = ld(1, otp(S1, 'me@example.com', 'GitHub', 1, 1, 2)) + ld(1, otp(S2, 'Mail Co:anna', '', 2, 2, 2)) + ld(1, otp(S3, 'hotp-one', 'X', 1, 1, 1)) + ld(1, otp(S1, 'md5', 'Y', 4, 1, 2)) + vi(2, 1) + vi(3, 1) + vi(4, 0)
link = 'otpauth-migration://offline?data=' + urllib.parse.quote(base64.b64encode(payload).decode(), safe='')
n, sk, got = parse(link + '\n')
check(n == 2 and got == [acc('GitHub', 'me@example.com', 0, 6, 30, S1), acc('Mail Co', 'anna', 1, 8, 30, S2)], 'Google Authenticator export (protobuf in base64, percent-encoded): TOTP kept, HOTP and MD5 skipped')
big = b''.join(ld(1, otp(bytes([i] * 20), f'user{i}', f'Svc{i}', 1, 1, 2)) for i in range(10))
link = 'otpauth-migration://offline?data=' + urllib.parse.quote(base64.b64encode(big).decode(), safe='')
n, sk, got = parse(link + '\n')
check(n == 10 and got[9] == acc('Svc9', 'user9', 0, 6, 30, bytes([9] * 20)), 'a batch of ten accounts')

# Aegis
aegis = {'version': 1, 'header': {'slots': None, 'params': None}, 'db': {'version': 2, 'entries': [
    {'type': 'totp', 'uuid': 'a', 'name': 'me@example.com', 'issuer': 'GitHub', 'note': '', 'favorite': False, 'icon': None, 'info': {'secret': b32(S1), 'algo': 'SHA1', 'digits': 6, 'period': 30}},
    {'type': 'totp', 'uuid': 'b', 'name': 'anna', 'issuer': 'Mail Ü Co', 'info': {'secret': b32(S2), 'algo': 'SHA256', 'digits': 8, 'period': 60}},
    {'type': 'hotp', 'uuid': 'c', 'name': 'counter', 'issuer': 'X', 'info': {'secret': b32(S1), 'algo': 'SHA1', 'digits': 6, 'counter': 1}},
    {'type': 'steam', 'uuid': 'd', 'name': 'game', 'issuer': 'Steam', 'info': {'secret': b32(S1), 'algo': 'SHA1', 'digits': 5, 'period': 30}}], 'groups': []}}
n, sk, got = parse(json.dumps(aegis, indent=2))
check(n == 2 and got == [acc('GitHub', 'me@example.com', 0, 6, 30, S1), acc('Mail Ü Co'.encode('latin-1').decode('latin-1'), 'anna', 1, 8, 60, S2)], 'Aegis export: totp kept, hotp and steam skipped (also with \\u escapes and a BOM)')
n, sk, got = parse('﻿' + json.dumps(aegis, ensure_ascii=True))
check(n == 2, 'the same with escaped non-ASCII and a byte order mark')
n, sk, got = parse(json.dumps({'version': 1, 'header': {'slots': [{}]}, 'db': 'Zm9v'}))
check(n < 1, 'an encrypted export of Aegis is not read (it says so with 0 accounts)')

# the form of the phone app
def form(code, name, issuer, digits, period, algo):
    out = run(f'F {hx(code)} {hx(name)} {hx(issuer)} {digits} {period} {hx(algo)}\n')
    return int(out[0]), out[1:]
r, a = form(b32(S1)[:8] + ' ' + b32(S1)[8:], 'my account', 'ACME', 7, 45, 'SHA256')
check(r == 1 and a == [acc('ACME', 'my account', 1, 7, 45, S1)], 'form: a long code with spaces, name, issuer, digits, period, algorithm')
r, a = form(uri1, '', '', 6, 30, 'SHA1')
check(r == 1 and a == [acc('GitHub', 'me@example.com', 0, 6, 30, S1)], 'form: an otpauth link in the field')
r, a = form(uri1, 'other name', '', 6, 30, 'SHA1')
check(r == 1 and a == [acc('GitHub', 'other name', 0, 6, 30, S1)], 'form: the name that is typed wins over the one in the link')
r, a = form('', '', '', 6, 30, 'SHA1')
check(r == 0, 'form: empty means nothing there')
r, a = form('this is not a key!', '', '', 6, 30, 'SHA1')
check(r == -1, 'form: a text that is no key is refused')

print('all checks passed' if not fails else 'SOME CHECKS FAILED')
sys.exit(1 if fails else 0)
