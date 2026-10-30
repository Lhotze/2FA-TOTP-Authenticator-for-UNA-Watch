#!/usr/bin/env python3
"""Puts the pictures of Tools/make_media.cpp together: python3 -I make_media.py <raw folder> <output folder>
Writes: screenshots/NN_name.png (480 x 480), round/NN_name.png (round, transparent corners), overview.png (one sheet with captions) and the animated picture
2FA-TOTP-Authenticator_demo.gif (round, 480 x 480)."""
import os, sys
from PIL import Image, ImageDraw, ImageFont
raw, out = sys.argv[1], sys.argv[2]
S = 480
CAPTION = {
    '00_choose_pin': 'First start: choose a PIN', '01_pin_again': 'Once more', '02_no_accounts': 'Unlocked, no accounts yet', '03_add_info': 'Add an account',
    '04_account_added': 'Pasted on the phone: added', '05_three_added': 'Or from a file', '06_codes': 'The code and its time',
    '07_next_account': 'L1 / L2: another account', '08_seconds_running_out': 'The next code is shown', '09_eight_digits': '8 digits, SHA-256',
    '10_sixty_seconds': 'Another period (60 s)', '11_all_codes': 'All codes at a glance', '12_menu': 'The menu', '13_menu_down': 'Settings',
    '14_remove_which': 'Remove an account', '15_remove_ask': 'Only here, after a question', '16_change_pin': 'Change the PIN', '17_locked': 'Locked', '18_codes_again': 'Unlocked again',
}
def load(p): return Image.open(p).convert('RGB')
def big(im): return im.resize((S, S), Image.NEAREST)
def rounded(im, bg=None):
    m = Image.new('L', (S * 4, S * 4), 0); ImageDraw.Draw(m).ellipse((0, 0, S * 4 - 1, S * 4 - 1), fill=255); m = m.resize((S, S), Image.LANCZOS)
    if bg is None:
        r = im.convert('RGBA'); r.putalpha(m); return r
    base = Image.new('RGB', (S, S), bg); base.paste(im, (0, 0), m); return base
os.makedirs(os.path.join(out, 'screenshots'), exist_ok=True); os.makedirs(os.path.join(out, 'round'), exist_ok=True)
shots = sorted(f[:-4] for f in os.listdir(os.path.join(raw, 'shots')) if f.endswith('.ppm'))
for n in shots:
    im = big(load(os.path.join(raw, 'shots', n + '.ppm')))
    im.save(os.path.join(out, 'screenshots', n + '.png'), optimize=True); rounded(im).save(os.path.join(out, 'round', n + '.png'), optimize=True)
font = ImageFont.truetype('/usr/share/fonts/liberation/LiberationSans-Regular.ttf', 20); title = ImageFont.truetype('/usr/share/fonts/liberation/LiberationSans-Bold.ttf', 34)
cols, cw, ch, pad = 5, 300, 350, 14
rows = (len(shots) + cols - 1) // cols
sheet = Image.new('RGB', (cols * cw + pad * 2, rows * ch + pad * 2 + 90), (30, 32, 38)); d = ImageDraw.Draw(sheet)
d.text((pad + 8, 24), '2FA-TOTP-Authenticator for the UNA watch (made-up accounts)', fill=(255, 255, 255), font=title)
for i, n in enumerate(shots):
    x = pad + (i % cols) * cw; y = 90 + pad + (i // cols) * ch
    sheet.paste(rounded(big(load(os.path.join(raw, 'shots', n + '.ppm'))), (30, 32, 38)).resize((cw - 20, cw - 20), Image.LANCZOS), (x + 10, y))
    cap = CAPTION.get(n, n[3:].replace('_', ' ')); w = d.textlength(cap, font=font); assert w < cw - 6, cap
    d.text((x + (cw - w) / 2, y + cw - 14), cap, fill=(215, 220, 230), font=font)
sheet.save(os.path.join(out, 'overview.png'), optimize=True)
frames, dur = [], []
for line in open(os.path.join(raw, 'frames.txt')):
    name, ms = line.split(); frames.append(rounded(big(load(os.path.join(raw, name))), (24, 26, 30))); dur.append(max(90, int(int(ms) * 0.8)))
joined, jd = [], []
for f, d_ in zip(frames, dur):
    if joined and joined[-1].tobytes() == f.tobytes(): jd[-1] += d_
    else: joined.append(f); jd.append(d_)
pal = [f.quantize(colors=64, method=Image.MEDIANCUT, dither=Image.NONE) for f in joined]
pal[0].save(os.path.join(out, '2FA-TOTP-Authenticator_demo.gif'), save_all=True, append_images=pal[1:], duration=jd, loop=0, optimize=True, disposal=1)
print(len(shots), 'screenshots,', len(joined), 'frames,', sum(jd) / 1000.0, 's')
