# 2FA-TOTP-Authenticator
One-time codes (**TOTP**, RFC 6238) for two-factor sign-in, on the UNA watch. SHA-1, SHA-256 and SHA-512, 6 to 8 digits, any period (the usual: SHA-1, 6 digits, 30 s).
## Using it
**Everywhere: L1 up, L2 down, R1 choose, R2 back.**

* **First start**: a PIN of 4 to 6 digits is chosen on a **wheel** at the left edge (L1 / L2 turn it, R1 picks the entry in the frame, DEL takes one away, OK is done; R2 deletes too). It is asked twice. **After the sixth digit the wheel jumps to OK by itself** (and from the second unlock on also after the fourth or fifth: the app remembers how long the last PIN was; that length is kept in `settings.dat`, in the clear). The PIN locks the file that holds the keys.
* **A password instead of a PIN (optional)**: the entry `ABC` on the wheel of digits leads to a wheel of small letters; there `Aa` gives the capitals and `123` digits and signs (`. - _ @ ! # $ % & * + = ? / : ,`), `abc` / `aa` / `PIN` lead back. 6 to 24 characters. A password and a PIN are the same thing for the vault (the text is the key), so **a vault made with a PIN of four digits opens as before**, and a password of only digits is a PIN. The next start comes up with the wheel that fits the last unlock (letters after a password, digits after a PIN); the other wheel is one entry away.
* **Every start**: the PIN or the password. The key is made first (20,000 rounds of HMAC-SHA-256; a progress bar shows it - on the PC less than a second, on the watch **not measured**).
* **The codes**: the code of one account big (`123 456`), the service in big letters (the size that the round screen allows at that height) and the user name above it, a bar and the seconds that are left below, and, when 10 seconds are left, the **next code**. L1 / L2: the account before / after. R1: the menu. R2: leave (after a question).
* **Menu**: ALL CODES (every account in a list with its code and a thin bar), ADD ACCOUNT, REMOVE ACCOUNT, CHANGE PIN, LOCK, SCREEN (ALWAYS ON keeps the backlight on), MENU (normal / large letters), ABOUT, EXIT.
* **The time**: the codes are made from the time of the watch, which it gets from the phone. Without it the screen says so instead of showing a wrong code.
* When the screen goes away the keys are wiped from the memory and the PIN is asked again.

## Adding an account
There is no camera, and entering a 32-letter key with four buttons is no fun. So:

1. **The field of the phone app (the "one-shot field")**. A service that shows a QR code usually also shows the same key as a long text ("can't scan the code?"). In the UNA phone app, open the settings of this app and paste that text into **Secret key or link** (an `otpauth://` link works too; name, service, digits, period and algorithm are optional). Save. **When you open the app on the watch** (or press R1 in ADD ACCOUNT), it reads the field **once**, puts the account into the vault, and **empties the field** (the whole configuration file is written over with zeros and written again without the values). The message says `THE SOURCE WAS ERASED`.
2. **A file `import.txt`** (any name that begins with `import`, `.txt`) in the folder of the app. One account on a line: an `otpauth://totp/...` link, **the export link of Google Authenticator** (`otpauth-migration://offline?data=...`, up to ten accounts at once), or `Name, KEY` (also `Name KEY`). Lines with `#` are ignored.
3. **A file `import.json`** with the **unencrypted export of Aegis**. (An encrypted export is not read: export it unencrypted, import it, delete the export on the phone.)

**If the app stopped in the middle of such a check on a start**, the next start leaves the form and the files alone (a message says so) so that a fault cannot keep the app from starting; ADD ACCOUNT > R1 tries again when you ask. The file is **written over with zeros and removed** after the accounts are in the vault and the vault is saved. If the vault cannot be saved, **nothing is erased** and the message says so. A key that is already there is not added twice; a service that wants 5 digits, a counter (HOTP), Steam and the like are skipped. At most 32 accounts.

**Removing** an account is only possible in the app (MENU > REMOVE ACCOUNT), after a question.

## How safe it is - read this
* The keys are in `vault.dat`, **encrypted**: PBKDF2-HMAC-SHA256 (20,000 rounds, a salt of its own) makes the key from the PIN or password, the text is XORed with a stream of HMAC-SHA256 blocks, and an HMAC-SHA256 over the file checks the PIN and that nothing was changed. The format is described in `Software/Apps/TouchGFX-GUI/gui/include/ta/Vault.hpp`. It is **made of common parts, not a method that others have reviewed**, and I wrote it: it has been compared with an independent Python implementation in both directions, but not audited.
* **A PIN is short** (a password of 8 or more mixed characters is much better; a longer password costs a few more presses). Whoever can copy `vault.dat` (it is in the file system that the phone and USB can read) can try all 10,000 to 100,000,000 PINs on a computer; with 20,000 rounds a 4-digit PIN falls in seconds, a 6-digit one in minutes. The PIN keeps the keys from being read out of the file by someone who does not take that trouble, and from being used on the watch. Use 6 digits that are not a date, or a password.
* **The field of the phone app is not secret storage** (the SDK says so: "plaintext on a FAT volume that is readable over USB and over BLE"). The key sits in `app_config.json` **from the moment the phone writes it until the app is opened on the watch**. After that the field is empty. The phone app may keep what you typed (its own memory); that is outside this app. And a file system does not really erase: the flash may hold older copies after the file was written over.
* Whoever has the watch and its file system can **delete** `vault.dat` (the keys are then gone; keep the recovery codes of your services).
* The codes are right only if the time is. A watch that has not been connected to the phone for long can be off by seconds.

## Licence
MIT (see `LICENSE`). The project skeleton comes from the MIT-licensed templates of the UNA SDK; see `THIRD-PARTY-LICENSES.md`. **The software comes without any warranty: it is a test version, it has not run on a watch yet and its encryption has not been reviewed by others** - do not rely on it as the only copy of your second factors.
