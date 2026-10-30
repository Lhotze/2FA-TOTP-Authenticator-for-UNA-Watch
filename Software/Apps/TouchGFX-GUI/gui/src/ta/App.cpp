/**
 ******************************************************************************
 * @file    App.cpp
 * @brief   The authenticator: PIN, the one-time codes, the list of accounts, adding and removing.
 ******************************************************************************
 */

#include <ta/App.hpp>
#include <ta/Draw.hpp>

#include "SDK/GUI/Button.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ta {

namespace {

constexpr const char *kSettingsFile = "settings.dat";
constexpr uint32_t kBacklightResendMs = 5000;
constexpr int kReadLight = 70;
constexpr uint32_t kUnlockStepMs = 90;                 // work per tick while the key is made (the ticks come ten times a second)
constexpr uint32_t kImportMax = 24 * 1024;


// The part of a row of the round screen that can be used: from xl to xr, for a row of text that is h high at y.
void rowSpan(int y, int h, int &xl, int &xr)
{
    int dy = std::abs(y - 120);
    const int dy2 = std::abs(y + h - 120);
    if (dy2 > dy) dy = dy2;
    const int r = 118;
    int r2 = r * r - dy * dy;
    if (r2 < 0) r2 = 0;
    const int hw = static_cast<int>(std::sqrt(static_cast<double>(r2)));
    xl = 120 - hw; xr = 120 + hw;
    if (xl < 10) xl = 10;
    if (xr > 230) xr = 230;
}

inline bool isPress(uint8_t k)
{
    using namespace SDK::GUI;
    return k == Button::L1_PRESS || k == Button::L2_PRESS || k == Button::R1_PRESS || k == Button::R2_PRESS;
}

inline bool startsImport(const char *name)                 // import.txt, import 2.json ... (any case)
{
    const char *p = "import";
    for (int i = 0; i < 6; ++i) { char c = name[i]; if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32); if (c != p[i]) return false; }
    return true;
}

} // namespace

App::App(Platform &p)
    : mP(p), mVault(), mMode(Mode::Pin), mExit(false), mDirty(true), mNow(0), mLight(0), mLarge(0), mLast(0), mLightOn(false), mLightMs(0)
    , mPinFor(PinFor::Unlock), mPin{}, mPinFirst{}, mPinN(0), mPinCursor(0), mLayer(Layer::Digits), mPinLen(0), mPwHint(0), mWrongTries(0), mChanging(false)
    , mCur(0), mTop(0), mCursor(0), mLastUtc(0), mMessage{}, mMessageNext(Mode::Codes), mRemoveIndex(0), mAboutPage(0)
{
}

// ---- files -------------------------------------------------------------------------------------------------------------------------------------
void App::loadSettings()
{
    File *f = mP.open(kSettingsFile, false);
    if (!f) return;
    char buf[128];
    uint32_t n = f->size();
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    const bool ok = f->readNext(buf, n);
    delete f;
    if (!ok) return;
    buf[n] = 0;
    for (char *line = buf; line && *line;) {
        char *next = std::strchr(line, '\n');
        if (next) *next++ = 0;
        if (!std::strncmp(line, "light=", 6)) mLight = std::atoi(line + 6) ? 1 : 0;
        else if (!std::strncmp(line, "menu=", 5)) mLarge = std::atoi(line + 5) ? 1 : 0;
        else if (!std::strncmp(line, "last=", 5)) mLast = std::atoi(line + 5);
        else if (!std::strncmp(line, "pw=", 3)) mPwHint = std::atoi(line + 3) ? 1 : 0;
        else if (!std::strncmp(line, "pinlen=", 7)) { mPinLen = std::atoi(line + 7); if (mPinLen < 0 || mPinLen > kPassMax) mPinLen = 0; }
        line = next;
    }
}

void App::saveSettings()
{
    char buf[64];
    const int n = std::snprintf(buf, sizeof(buf), "light=%d\nmenu=%d\nlast=%d\npw=%d\npinlen=%d\n", mLight, mLarge, mCur, mPwHint, mPinLen);
    if (File *f = mP.open(kSettingsFile, true)) { f->write(buf, static_cast<uint32_t>(n)); delete f; }
}

// A file with keys in it is written over with zeros before it is removed (as far as a file system lets one: the flash may still hold an older copy).
void App::wipeFile(const char *name)
{
    uint32_t size = 0;
    if (File *f = mP.open(name, false)) { size = f->size(); delete f; }
    if (size > 0) {
        if (File *w = mP.open(name, true)) {
            uint8_t zeros[256] = {};
            for (uint32_t done = 0; done < size; done += sizeof(zeros)) w->write(zeros, size - done < sizeof(zeros) ? size - done : static_cast<uint32_t>(sizeof(zeros)));
            delete w;
        }
    }
    mP.remove(name);
}

uint32_t App::clock() { return mP.utc(); }

void App::currentCode(char *out, size_t cap)
{
    out[0] = 0;
    const uint32_t u = clock();
    if (!mVault.unlocked() || mVault.count() == 0 || u == 0) return;
    totp(mVault.at(mCur), u, out, cap);
}

// ---- start, lock -------------------------------------------------------------------------------------------------------------------------------
void App::start()
{
    mVault.stir(mP.nowMs(), mP.utc());
    loadSettings();
    mWrongTries = 0;
    if (Vault::fileExists(mP)) openPin(PinFor::Unlock);
    else openPin(PinFor::New);
}

void App::suspend(uint32_t nowMs)
{
    mNow = nowMs;
    if (mVault.unlocked()) saveSettings();
    mVault.lock();
    if (mLightOn) { mP.backlight(false, 100); mLightOn = false; }
    if (!mExit) { if (Vault::fileExists(mP)) openPin(PinFor::Unlock); else openPin(PinFor::New); }
}

void App::updateLight(bool force)
{
    if (mLight) {
        if (force || !mLightOn || mNow - mLightMs >= kBacklightResendMs) { mP.backlight(true, kReadLight); mLightOn = true; mLightMs = mNow ? mNow : 1; }
    } else if (mLightOn) { mP.backlight(false, 100); mLightOn = false; }
}

void App::openMode(Mode m)
{
    mMode = m;
    mDirty = true;
}

void App::showMessage(const char *l1, const char *l2, const char *l3, Mode next)
{
    std::snprintf(mMessage[0], sizeof(mMessage[0]), "%s", l1);
    std::snprintf(mMessage[1], sizeof(mMessage[1]), "%s", l2);
    std::snprintf(mMessage[2], sizeof(mMessage[2]), "%s", l3);
    mMessageNext = next;
    openMode(Mode::Message);
}

// ---- the PIN (or a password) -------------------------------------------------------------------------------------------------------------------
// The wheel has a layer of its own for what is typed: digits (the PIN), small letters, capitals, digits and signs. The entries that change the layer are on the wheel.
namespace {
const char kSmall[] = "abcdefghijklmnopqrstuvwxyz";
const char kCaps[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char kDigits[] = "0123456789";
const char kSigns[] = ".-_@!#$%&*+=?/:,";                  // 16
}

int App::wheelSize() const
{
    return mLayer == Layer::Digits ? 13 : 30;
}

App::WheelEntry App::wheelEntry(int i) const
{
    WheelEntry e = {};
    auto layerTo = [&](const char *label, char to) { std::snprintf(e.label, sizeof(e.label), "%s", label); e.kind = 3; e.ch = to; };
    auto character = [&](char c) { e.label[0] = c; e.label[1] = 0; e.kind = 0; e.ch = c; };
    auto del = [&]() { std::snprintf(e.label, sizeof(e.label), "DEL"); e.kind = 1; };
    auto ok = [&]() { std::snprintf(e.label, sizeof(e.label), "OK"); e.kind = 2; };
    switch (mLayer) {
        case Layer::Digits:
            if (i < 10) character(kDigits[i]); else if (i == 10) layerTo("ABC", 1); else if (i == 11) del(); else ok();
            break;
        case Layer::Lower:
        case Layer::Upper:
            if (i < 26) character((mLayer == Layer::Lower ? kSmall : kCaps)[i]);
            else if (i == 26) { if (mLayer == Layer::Lower) layerTo("Aa", 2); else layerTo("aa", 1); }
            else if (i == 27) layerTo("123", 3);
            else if (i == 28) del(); else ok();
            break;
        case Layer::Num:
            if (i < 10) character(kDigits[i]); else if (i < 26) character(kSigns[i - 10]);
            else if (i == 26) layerTo("abc", 1);
            else if (i == 27) layerTo("PIN", 0);
            else if (i == 28) del(); else ok();
            break;
    }
    return e;
}

int App::wheelIndexOfChar(char c) const
{
    for (int i = 0; i < wheelSize(); ++i) { const WheelEntry e = wheelEntry(i); if (e.kind == 0 && e.ch == c) return i; }
    return -1;
}

int App::wheelIndexOfLabel(const char *label) const
{
    for (int i = 0; i < wheelSize(); ++i) if (!std::strcmp(wheelEntry(i).label, label)) return i;
    return -1;
}

void App::openPin(PinFor purpose)
{
    const bool again = purpose == PinFor::NewAgain || purpose == PinFor::ChangeAgain;
    bool lettersFirst = mPwHint != 0 && purpose == PinFor::Unlock;                    // the unlock is made ready for what was typed last time
    if (again) { for (const char *c = mPinFirst; *c; ++c) if (*c < '0' || *c > '9') lettersFirst = true; }   // and the second time like the first
    mPinFor = purpose;
    mPinN = 0; mPin[0] = 0;
    mLayer = lettersFirst ? Layer::Lower : Layer::Digits;
    mPinCursor = 0;
    if (purpose == PinFor::New || purpose == PinFor::ChangeNew) mPinFirst[0] = 0;
    openMode(Mode::Pin);
}

void App::addChar(char c)
{
    const int cap = (mPinFor == PinFor::Unlock || mLayer != Layer::Digits) ? kPassMax : kPinMaxNew;
    if (mPinN >= cap) return;
    // a password has letters or signs somewhere, a PIN is digits: a PIN of the wheel of digits is at most 6 when it is made
    if (mPinFor != PinFor::Unlock && mPinN >= kPinMaxNew && mLayer == Layer::Digits) return;
    mPin[mPinN++] = c; mPin[mPinN] = 0;
    // after the last character the wheel goes to OK: when the length is known (from the last unlock) or at the end of what a PIN can be
    const int target = mPinFor == PinFor::Unlock ? mPinLen : (mLayer == Layer::Digits ? kPinMaxNew : 0);
    if ((target > 0 && mPinN == target) || mPinN == kPassMax) mPinCursor = wheelSize() - 1;
}

void App::pinKey(uint8_t key)
{
    using namespace SDK::GUI;
    const int n = wheelSize();
    if (key == Button::L1_PRESS) mPinCursor = (mPinCursor + n - 1) % n;
    else if (key == Button::L2_PRESS) mPinCursor = (mPinCursor + 1) % n;
    else if (key == Button::R2_PRESS) {
        if (mPinN > 0) mPin[--mPinN] = 0;
        else if (mPinFor == PinFor::Unlock || mPinFor == PinFor::New) { openMode(Mode::ConfirmExit); return; }
        else openMode(Mode::Menu);                                       // a PIN change: back
    } else if (key == Button::R1_PRESS) {
        const WheelEntry e = wheelEntry(mPinCursor);
        if (e.kind == 0) addChar(e.ch);
        else if (e.kind == 1) { if (mPinN > 0) mPin[--mPinN] = 0; }
        else if (e.kind == 3) {                                          // another layer: the cursor stays on the entry that leads back
            const Layer from = mLayer;
            mLayer = static_cast<Layer>(e.ch);
            mPinCursor = 0;
            for (int i = 0; i < wheelSize(); ++i) {
                const WheelEntry b = wheelEntry(i);
                if (b.kind == 3 && static_cast<Layer>(b.ch) == from) { mPinCursor = i; break; }
            }
        } else { pinChosen(); return; }
    }
    mDirty = true;
}

static bool digitsOnly(const char *s) { for (; *s; ++s) if (*s < '0' || *s > '9') return false; return true; }

void App::pinChosen()
{
    const bool digits = digitsOnly(mPin);
    const int least = mPinFor == PinFor::Unlock ? kPinMin : (digits ? kPinMin : kPassMin);
    if (mPinN < least) { mDirty = true; return; }
    switch (mPinFor) {
        case PinFor::Unlock:
            if (!mVault.beginUnlock(mP, mPin)) { openMode(Mode::ResetAsk); return; }
            mChanging = false;
            openMode(Mode::Unlocking);
            return;
        case PinFor::New:
        case PinFor::ChangeNew:
            std::memcpy(mPinFirst, mPin, sizeof(mPin));
            openPin(mPinFor == PinFor::New ? PinFor::NewAgain : PinFor::ChangeAgain);
            return;
        case PinFor::NewAgain:
        case PinFor::ChangeAgain:
            if (std::strcmp(mPin, mPinFirst) != 0) {
                showMessage("THE ENTRIES DIFFER", "TRY AGAIN", "", Mode::Pin);
                mPinFor = mPinFor == PinFor::NewAgain ? PinFor::New : PinFor::ChangeNew;
                mPinN = 0; mPin[0] = 0; mPinFirst[0] = 0; mLayer = Layer::Digits; mPinCursor = 0;
                return;
            }
            if (mPinFor == PinFor::NewAgain) { mVault.beginNew(mPin); mChanging = false; }
            else { mVault.beginPinChange(mPin); mChanging = true; }
            std::memset(mPinFirst, 0, sizeof(mPinFirst));
            openMode(Mode::Unlocking);
            return;
    }
}

// the vault is open: the form of the phone app and the import files are looked at, then the codes
void App::unlocked_()
{
    mWrongTries = 0;
    mCur = mLast >= 0 && mLast < mVault.count() ? mLast : 0;
    mTop = 0;
    const int added = checkImports(true);
    if (added < 0) return;                                              // a message is shown
    openMode(Mode::Codes);
}

// ---- adding ------------------------------------------------------------------------------------------------------------------------------------
// The one-shot form of the phone app (a long code that was pasted there) and files like import.txt in the folder. What was read is put into the vault, and only
// when the vault is saved the form is emptied and the files are written over with zeros and removed. Returns the number added, -1 if a message was shown.
int App::checkImports(bool automatic)
{
    // A marker file while this runs: if the app stopped in the middle (nobody can say why without logs from the watch), the next start does not walk into it again.
    if (automatic && mP.exists("check.lock")) {
        mP.remove("check.lock");
        showMessage("THE LAST CHECK STOPPED", "FOR NEW ACCOUNTS", "TRY: MENU > ADD ACCOUNT > R1", Mode::Codes);
        return -1;
    }
    if (File *m = mP.open("check.lock", true)) { const char one = '1'; m->write(&one, 1); delete m; }
    int added = 0, refused = 0, bad = 0, full = 0;
    bool anything = false;
    Form form;
    bool formSeen = false;
    if (mP.readForm(form)) {
        formSeen = true;
        Account a;
        const int r = parseForm(form.code, form.name, form.issuer, form.digits, form.period, form.algo, a);
        if (r == 1) { if (mVault.count() >= kMaxAccounts) ++full; else if (mVault.add(a)) ++added; else ++refused; anything = true; }
        else if (r < 0) { ++bad; anything = true; }
    }
    char names[kMaxFiles][kNameMax];
    static char fileNames[kMaxFiles][kNameMax];
    int fileCount = 0;
    static const char *const exts[2] = { ".txt", ".json" };
    for (int e = 0; e < 2; ++e) {
        const int n = mP.listFiles(exts[e], names, kMaxFiles);
        for (int i = 0; i < n && fileCount < kMaxFiles; ++i) if (startsImport(names[i])) std::memcpy(fileNames[fileCount++], names[i], kNameMax);
    }
    static char buf[kImportMax + 1];
    static Account found[kMaxAccounts];
    bool filesRead[kMaxFiles] = {};
    for (int i = 0; i < fileCount; ++i) {
        File *f = mP.open(fileNames[i], false);
        if (!f) continue;
        uint32_t n = f->size();
        if (n > kImportMax) n = kImportMax;
        const bool ok = f->read(0, buf, n);
        delete f;
        if (!ok) continue;
        buf[n] = 0;
        int skipped = 0;
        const int c = parseText(buf, static_cast<int>(n), found, kMaxAccounts, &skipped);
        for (int k = 0; k < c; ++k) { if (mVault.count() >= kMaxAccounts) ++full; else if (mVault.add(found[k])) ++added; else ++refused; }
        bad += skipped;
        if (c < 0) ++bad;
        filesRead[i] = true;
        anything = true;
    }
    std::memset(buf, 0, sizeof(buf));
    if (!anything) { mP.remove("check.lock"); return 0; }
    mVault.stir(mP.nowMs(), mP.utc());
    if (added > 0 && !mVault.save(mP)) {                                // nothing is wiped: the keys are still where they came from
        mP.remove("check.lock");
        showMessage("COULD NOT SAVE", "THE ACCOUNTS", "NOTHING WAS ERASED", Mode::Codes);
        return -1;
    }
    if (formSeen) mP.clearForm();
    for (int i = 0; i < fileCount; ++i) if (filesRead[i]) wipeFile(fileNames[i]);
    if (added > 0) mCur = mVault.count() - added;                      // the first of the new ones
    char l1[40], l2[40] = "", l3[40] = "";
    if (added > 0) std::snprintf(l1, sizeof(l1), added == 1 ? "1 ACCOUNT ADDED" : "%d ACCOUNTS ADDED", added);
    else std::snprintf(l1, sizeof(l1), "NOTHING ADDED");
    if (full > 0) std::snprintf(l2, sizeof(l2), "THE LIST IS FULL (%d)", kMaxAccounts);
    else if (refused > 0) std::snprintf(l2, sizeof(l2), "%d ALREADY THERE", refused);
    if (bad > 0) std::snprintf(l3, sizeof(l3), "%d NOT USABLE", bad);
    if (formSeen || fileCount) { if (!l3[0]) std::snprintf(l3, sizeof(l3), "THE SOURCE WAS ERASED"); }
    mP.remove("check.lock");
    showMessage(l1, l2, l3, Mode::Codes);
    return -1;
}

// ---- lists -------------------------------------------------------------------------------------------------------------------------------------
int App::listCount() const
{
    switch (mMode) {
        case Mode::Menu: return 9;
        case Mode::RemoveList: return mVault.count();
        default: return 0;
    }
}

void App::label(int i, char *out, size_t cap)
{
    out[0] = 0;
    if (mMode == Mode::Menu) {
        static const char *const items[9] = { "ALL CODES", "ADD ACCOUNT", "REMOVE ACCOUNT", "CHANGE PIN", "LOCK", "SCREEN", "MENU SIZE", "ABOUT", "EXIT" };
        if (i == 5) std::snprintf(out, cap, "SCREEN: %s", mLight ? "ALWAYS ON" : "AUTO OFF");
        else if (i == 6) std::snprintf(out, cap, "MENU: %s", mLarge ? "LARGE" : "NORMAL");
        else std::snprintf(out, cap, "%s", items[i]);
    } else if (mMode == Mode::RemoveList) {
        const Account &a = mVault.at(i);
        std::snprintf(out, cap, "%s%s%s", a.issuer, a.issuer[0] && a.name[0] ? " " : "", a.name);
    }
}

int App::menuIndexOf(const char *start)
{
    const Mode keep = mMode;
    mMode = Mode::Menu;
    int found = -1;
    for (int i = 0; i < listCount() && found < 0; ++i) {
        char buf[48];
        label(i, buf, sizeof(buf));
        if (!std::strncmp(buf, start, std::strlen(start))) found = i;
    }
    mMode = keep;
    return found;
}

void App::listKey(uint8_t key)
{
    using namespace SDK::GUI;
    const int n = listCount();
    if (n > 0) {
        if (key == Button::L1_PRESS) mCursor = (mCursor + n - 1) % n;
        else if (key == Button::L2_PRESS) mCursor = (mCursor + 1) % n;
    }
    mDirty = true;
}

void App::listChoose()
{
    if (mMode == Mode::Menu) {
        switch (mCursor) {
            case 0: mTop = 0; openMode(Mode::All); break;
            case 1: mAboutPage = 0; openMode(Mode::AddInfo); break;
            case 2:
                if (mVault.count() == 0) showMessage("NO ACCOUNTS", "", "", Mode::Menu);
                else { mCursor = 0; openMode(Mode::RemoveList); }
                break;
            case 3: openPin(PinFor::ChangeNew); break;
            case 4: mVault.lock(); mWrongTries = 0; openPin(PinFor::Unlock); break;
            case 5: mLight = 1 - mLight; saveSettings(); updateLight(true); mDirty = true; break;
            case 6: mLarge = 1 - mLarge; saveSettings(); mDirty = true; break;
            case 7: mAboutPage = 0; openMode(Mode::About); break;
            case 8: openMode(Mode::ConfirmExit); break;
            default: break;
        }
    } else if (mMode == Mode::RemoveList) {
        if (mVault.count() > 0) { mRemoveIndex = mCursor; openMode(Mode::RemoveAsk); }
    }
}

// ---- keys --------------------------------------------------------------------------------------------------------------------------------------
void App::onKey(uint8_t key, uint32_t nowMs)
{
    using namespace SDK::GUI;
    if (!isPress(key)) return;
    mNow = nowMs;
    mVault.stir(nowMs, static_cast<uint32_t>(key));
    if (mLight) mLightMs = 0;
    switch (mMode) {
        case Mode::Pin: pinKey(key); break;
        case Mode::Unlocking: break;
        case Mode::Codes:
            if (mVault.count() > 0) {
                if (key == Button::L1_PRESS) { mCur = (mCur + mVault.count() - 1) % mVault.count(); mDirty = true; }
                else if (key == Button::L2_PRESS) { mCur = (mCur + 1) % mVault.count(); mDirty = true; }
            }
            if (key == Button::R1_PRESS) { mCursor = 0; openMode(Mode::Menu); }
            else if (key == Button::R2_PRESS) openMode(Mode::ConfirmExit);
            break;
        case Mode::All: {
            const int n = mVault.count();
            if (n > 0) {
                if (key == Button::L1_PRESS) { mCur = (mCur + n - 1) % n; mDirty = true; }
                else if (key == Button::L2_PRESS) { mCur = (mCur + 1) % n; mDirty = true; }
            }
            if (key == Button::R1_PRESS) openMode(Mode::Codes);
            else if (key == Button::R2_PRESS) { mCursor = 0; openMode(Mode::Menu); }
            break;
        }
        case Mode::Menu:
        case Mode::RemoveList:
            if (key == Button::L1_PRESS || key == Button::L2_PRESS) listKey(key);
            else if (key == Button::R1_PRESS) listChoose();
            else if (key == Button::R2_PRESS) { if (mMode == Mode::Menu) openMode(Mode::Codes); else { mCursor = 2; openMode(Mode::Menu); } }
            break;
        case Mode::RemoveAsk:
            if (key == Button::R1_PRESS) {
                mVault.removeAt(mRemoveIndex);
                const bool ok = mVault.save(mP);
                if (mCur >= mVault.count()) mCur = mVault.count() > 0 ? mVault.count() - 1 : 0;
                mCursor = 0;
                if (!ok) showMessage("COULD NOT SAVE", "", "", Mode::Menu);
                else if (mVault.count() == 0) showMessage("ACCOUNT REMOVED", "NO ACCOUNTS LEFT", "", Mode::Menu);
                else openMode(Mode::RemoveList);
            } else if (key == Button::R2_PRESS) openMode(Mode::RemoveList);
            break;
        case Mode::AddInfo:
            if (key == Button::R1_PRESS) {                                // look at the form and the files now
                const int r = checkImports(false);
                if (r == 0) showMessage("NOTHING NEW FOUND", "", "", Mode::AddInfo);
                else if (r > 0) openMode(Mode::Codes);
            } else if (key == Button::R2_PRESS) { mCursor = 1; openMode(Mode::Menu); }
            else if (key == Button::L1_PRESS) { if (mAboutPage > 0) --mAboutPage; mDirty = true; }
            else if (key == Button::L2_PRESS) { ++mAboutPage; mDirty = true; }
            break;
        case Mode::About:
            if (key == Button::L1_PRESS) { if (mAboutPage > 0) --mAboutPage; mDirty = true; }
            else if (key == Button::L2_PRESS) { ++mAboutPage; mDirty = true; }
            else { mCursor = 7; openMode(Mode::Menu); }
            break;
        case Mode::ConfirmExit:
            if (key == Button::R1_PRESS) { if (mVault.unlocked()) saveSettings(); mVault.lock(); mExit = true; }
            else if (key == Button::R2_PRESS) { if (mVault.unlocked()) openMode(Mode::Codes); else openMode(Mode::Pin); }
            break;
        case Mode::ResetAsk:
            if (key == Button::R1_PRESS) { mP.remove(kVaultFile); mP.remove("vault.tmp"); openPin(PinFor::New); }
            else if (key == Button::R2_PRESS) { mExit = true; }
            break;
        case Mode::Message:
            if (key == Button::R1_PRESS || key == Button::R2_PRESS) openMode(mMessageNext);
            break;
    }
}

// ---- drawing -----------------------------------------------------------------------------------------------------------------------------------
void App::group(const char *digits, char *out)
{
    const int n = static_cast<int>(std::strlen(digits));
    const int first = (n + 1) / 2 - (n == 7 ? 1 : 0);                   // 6: 3 + 3, 7: 3 + 4, 8: 4 + 4
    int o = 0;
    for (int i = 0; i < n; ++i) { if (i == first) out[o++] = ' '; out[o++] = digits[i]; }
    out[o] = 0;
}

void App::drawBox(uint8_t *fb, const char *l1, const char *l2, const char *l3, const char *hint)
{
    using namespace draw;
    std::memset(fb, BLACK, static_cast<size_t>(kW) * kH);
    frame(fb, 24, 70, 192, 100, 2, TEAL);
    textCentered(fb, 120, 82, l1, 1, YELLOW);
    textCentered(fb, 120, 104, l2, 1, WHITE);
    textCentered(fb, 120, 124, l3, 0, GRAY);
    textCentered(fb, 120, 150, hint, 0, GRAY);
}

void App::drawUnlocking(uint8_t *fb)
{
    using namespace draw;
    std::memset(fb, BLACK, static_cast<size_t>(kW) * kH);
    textCentered(fb, 120, 70, mChanging ? "MAKING THE NEW KEY" : "UNLOCKING", 1, TEAL_L);
    const int pct = mVault.progress();
    frame(fb, 40, 104, 160, 18, 2, TEAL);
    fillRect(fb, 44, 108, (152 * pct) / 100, 10, GREEN);
    char t[16];
    std::snprintf(t, sizeof(t), "%d%%", pct);
    textCentered(fb, 120, 132, t, 1, WHITE);
}

void App::drawPin(uint8_t *fb)
{
    using namespace draw;
    std::memset(fb, BLACK, static_cast<size_t>(kW) * kH);
    constexpr int kSlots = 9;
    constexpr double kPi = 3.14159265358979, kRadius = 94.0, kStepDeg = 15.0;
    const int n = wheelSize();
    frame(fb, 8, 101, 52, 38, 2, YELLOW);
    for (int k = 0; k < kSlots; ++k) {
        const int off = k - 4;
        const int idx = ((mPinCursor + off) % n + n) % n;
        const WheelEntry e = wheelEntry(idx);
        const double a = (180.0 + off * kStepDeg) * kPi / 180.0;
        const int cx = static_cast<int>(120.0 + kRadius * std::cos(a) + 0.5), cy = static_cast<int>(120.0 - kRadius * std::sin(a) + 0.5);
        const int font = off == 0 ? 3 : 2;
        const uint8_t color = off == 0 ? YELLOW : (std::abs(off) == 1 ? WHITE : (std::abs(off) == 2 ? GRAY : GRAY_D));
        const BitFont &f = kFonts[font];
        const int ln = static_cast<int>(std::strlen(e.label));
        text(fb, cx - ln * f.w / 2, cy - f.h / 2, e.label, ln, font, color);
    }
    const bool pwd = mLayer != Layer::Digits || !digitsOnly(mPin);
    const char *title;
    if (mPinFor == PinFor::Unlock) title = pwd ? "PASSWORD" : "ENTER PIN";
    else if (mPinFor == PinFor::New || mPinFor == PinFor::ChangeNew) title = pwd ? "NEW PASSWORD" : "NEW PIN";
    else title = pwd ? "ONCE MORE" : "PIN AGAIN";
    textCentered(fb, 150, 34, title, 1, YELLOW);
    // what is typed: dots (more than 16: a number)
    if (mPinN <= 16) {
        const int slots = mPinN > 4 ? mPinN : 4;
        const int w = slots * 10 - 2;
        int x = 150 - w / 2;
        for (int i = 0; i < slots; ++i, x += 10) {
            if (i < mPinN) fillRect(fb, x, 86, 8, 12, WHITE);
            else frame(fb, x, 86, 8, 12, 1, GRAY_D);
        }
    } else {
        char t[24];
        std::snprintf(t, sizeof(t), "%d CHARACTERS", mPinN);
        textCentered(fb, 150, 88, t, 1, WHITE);
    }
    char t[40];
    if (mPinFor == PinFor::Unlock && mWrongTries > 0) { std::snprintf(t, sizeof(t), "WRONG (%d)", mWrongTries); textCentered(fb, 150, 124, t, 1, RED); }
    else if (mPinFor == PinFor::Unlock) textCentered(fb, 150, 124, mPinLen > 0 ? "" : "4 - 24 CHARACTERS", 0, GRAY);
    else textCentered(fb, 150, 124, mLayer == Layer::Digits ? "4 - 6 DIGITS" : "6 - 24 CHARACTERS", 0, GRAY);
    if (mPinFor == PinFor::New) textCentered(fb, 150, 150, "LOCKS YOUR KEYS", 0, GRAY_D);
    else if (mPinFor == PinFor::ChangeNew || mPinFor == PinFor::NewAgain || mPinFor == PinFor::ChangeAgain) textCentered(fb, 150, 150, "CHOOSE ONE YOU KNOW", 0, GRAY_D);
    textCentered(fb, 150, 202, "R1 PICK  R2 DELETE", 0, GRAY_D);
}

void App::drawCodes(uint8_t *fb)
{
    using namespace draw;
    std::memset(fb, BLACK, static_cast<size_t>(kW) * kH);
    if (mVault.count() == 0) {
        textCentered(fb, 120, 70, "NO ACCOUNTS YET", 1, YELLOW);
        textCentered(fb, 120, 100, "R1: MENU > ADD ACCOUNT", 0, GRAY);
        textCentered(fb, 120, 116, "PASTE THE SECRET KEY", 0, GRAY);
        textCentered(fb, 120, 128, "IN THE PHONE APP", 0, GRAY);
        return;
    }
    const Account &a = mVault.at(mCur);
    // the service and the name: as big as the round screen allows at that height
    auto fitted = [&](const char *str, int y, int biggest, int smallest) {
        int font = biggest;
        for (; font > smallest; --font) {
            int xl, xr;
            rowSpan(y, kFonts[font].h, xl, xr);
            if (static_cast<int>(std::strlen(str)) * kFonts[font].w <= xr - xl - 8) break;
        }
        return font;
    };
    char head[40];
    std::snprintf(head, sizeof(head), "%s", a.issuer[0] ? a.issuer : a.name);
    const int hf = fitted(head, 20, 3, 1);
    {
        int xl, xr;
        rowSpan(20, kFonts[hf].h, xl, xr);
        int chars = (xr - xl - 8) / kFonts[hf].w;
        if (chars < static_cast<int>(sizeof(head)) && static_cast<int>(std::strlen(head)) > chars) head[chars] = 0;
    }
    textCentered(fb, 120, 20, head, hf, TEAL_L);
    if (a.issuer[0] && a.name[0]) {
        char nm[40];
        std::snprintf(nm, sizeof(nm), "%s", a.name);
        const int nf = fitted(nm, 46, 2, 0);
        int xl, xr;
        rowSpan(46, kFonts[nf].h, xl, xr);
        const int chars = (xr - xl - 8) / kFonts[nf].w;
        if (static_cast<int>(std::strlen(nm)) > chars) nm[chars] = 0;
        textCentered(fb, 120, 46, nm, nf, GRAY);
    }
    const uint32_t utc = clock();
    if (utc == 0) {
        textCentered(fb, 120, 96, "THE TIME IS NOT SET", 1, RED);
        textCentered(fb, 120, 120, "CONNECT THE WATCH", 0, GRAY);
        textCentered(fb, 120, 132, "TO THE PHONE APP", 0, GRAY);
    } else {
        char code[16], shown[24];
        totp(a, utc, code, sizeof(code));
        group(code, shown);
        const int n = static_cast<int>(std::strlen(shown));
        textScaled(fb, 120 - n * 20 / 2, 84, shown, n, 3, 2, WHITE);
        const int left = totpSecondsLeft(a, utc);
        const int period = a.period ? a.period : 30;
        const uint8_t col = left <= 5 ? RED : (left <= 10 ? YELLOW : TEAL);
        frame(fb, 40, 140, 160, 10, 1, GRAY_D);
        fillRect(fb, 42, 142, (156 * left) / period, 6, col);
        char t[16];
        std::snprintf(t, sizeof(t), "%d S", left);
        textCentered(fb, 120, 156, t, 1, col == TEAL ? GRAY : col);
        if (left <= 10) {                                               // the next code, when the time of this one is nearly over
            char nx[16], nshown[24], line[40];
            Account b = a;
            totp(b, utc + static_cast<uint32_t>(left), nx, sizeof(nx));
            group(nx, nshown);
            std::snprintf(line, sizeof(line), "NEXT  %s", nshown);
            textCentered(fb, 120, 178, line, 1, GRAY);
        }
    }
    char pos[16];
    std::snprintf(pos, sizeof(pos), "%d / %d", mCur + 1, mVault.count());
    textCentered(fb, 120, 204, pos, 0, GRAY_D);
}

void App::drawAll(uint8_t *fb)
{
    using namespace draw;
    std::memset(fb, BLACK, static_cast<size_t>(kW) * kH);
    textCentered(fb, 120, 11, "ALL CODES", 0, TEAL_L);
    const int n = mVault.count();
    const int rows = 5, rowH = 32;
    if (mCur < mTop) mTop = mCur;
    if (mCur >= mTop + rows) mTop = mCur - rows + 1;
    const uint32_t utc = clock();
    for (int k = 0; k < rows && mTop + k < n; ++k) {
        const int i = mTop + k;
        const int y = 32 + k * rowH;
        int xl, xr;
        rowSpan(y - 2, rowH - 4, xl, xr);
        const Account &a = mVault.at(i);
        if (i == mCur) fillRect(fb, xl + 1, y - 2, xr - xl - 2, rowH - 4, TEAL_D);
        char lab[40];
        std::snprintf(lab, sizeof(lab), "%s", a.issuer[0] ? a.issuer : a.name);
        const int room = (xr - xl - 16) / 7;
        char code[16] = "", shown[24] = "";
        if (utc) { totp(a, utc, code, sizeof(code)); group(code, shown); }
        else std::snprintf(shown, sizeof(shown), "------");
        const int cl = static_cast<int>(std::strlen(shown));
        const int labelChars = (room - cl) > 3 ? room - cl : 3;
        lab[labelChars < static_cast<int>(sizeof(lab)) ? labelChars : static_cast<int>(sizeof(lab)) - 1] = 0;
        text(fb, xl + 6, y + 2, lab, 2, i == mCur ? WHITE : GRAY);
        text(fb, xr - 6 - cl * 7, y + 2, shown, 2, i == mCur ? YELLOW : TEAL_L);
        if (utc) {                                                      // the time that is left: a thin line under the row
            const int left = totpSecondsLeft(a, utc), period = a.period ? a.period : 30;
            fillRect(fb, xl + 6, y + 18, ((xr - xl - 12) * left) / period, 2, left <= 5 ? RED : (left <= 10 ? YELLOW : TEAL));
        }
    }
    if (mTop > 0) fillRect(fb, 116, 26, 8, 2, GRAY);
    if (mTop + rows < n) fillRect(fb, 116, 194, 8, 2, GRAY);
    textCentered(fb, 120, 214, "R1 OPEN  R2 MENU", 0, GRAY_D);
}

void App::drawList(uint8_t *fb, const char *title)
{
    using namespace draw;
    std::memset(fb, BLACK, static_cast<size_t>(kW) * kH);
    const int n = listCount();
    if (mCursor >= n) mCursor = n > 0 ? n - 1 : 0;
    const int rows = listRows(), rowH = listRowH(), font = listFont();
    const BitFont &lf = kFonts[font];
    const int y0 = mLarge ? 30 : 32;
    if (mCursor < mTop) mTop = mCursor;
    if (mCursor >= mTop + rows) mTop = mCursor - rows + 1;
    if (mTop < 0) mTop = 0;
    textCentered(fb, 120, 11, title, 0, TEAL_L);
    int xmin = 0;
    for (int k = 0; k < rows && mTop + k < n; ++k) {
        int xl, xr;
        rowSpan(y0 + k * rowH - 2, rowH - 4, xl, xr);
        if (xl > xmin) xmin = xl;
    }
    for (int k = 0; k < rows; ++k) {
        const int i = mTop + k;
        if (i >= n) break;
        const int top0 = y0 + k * rowH - 2;
        int xl, xr;
        rowSpan(top0, rowH - 4, xl, xr);
        const bool sel = i == mCursor;
        if (sel) fillRect(fb, xl + 1, top0, xr - xl - 2, rowH - 4, TEAL_D);
        char buf[64];
        label(i, buf, sizeof(buf));
        const int chars = (xr - xmin - 10) / lf.w;
        const int len = static_cast<int>(std::strlen(buf));
        text(fb, xmin + 6, top0 + (rowH - 4 - lf.h) / 2, buf, len < chars ? len : chars, font, sel ? WHITE : GRAY);
    }
    if (mTop > 0) fillRect(fb, 116, 26, 8, 2, GRAY);
    if (mTop + rows < n) fillRect(fb, 116, y0 + rows * rowH - 2, 8, 2, GRAY);
    textCentered(fb, 120, 214, "R1 OK  R2 BACK", 0, GRAY_D);
}

// words in rows of the round screen
static int wrapRows(uint8_t *fb, const char *s, int firstY, int rowH, int font, int skipRows, int maxRows, uint8_t color, int pad)
{
    using namespace draw;
    const BitFont &f = kFonts[font];
    const int len = static_cast<int>(std::strlen(s));
    int pos = 0, row = 0;
    while (pos < len) {
        const int y = firstY + (row - skipRows) * rowH;
        int xl, xr;
        rowSpan(y, f.h, xl, xr);
        const int chars = (xr - xl - 2 * pad) / f.w;
        int i = pos, lastSpace = -1;
        while (i < len && i - pos < chars) { if (s[i] == ' ') lastSpace = i; ++i; }
        int end = i >= len ? len : (s[i] == ' ' ? i : (lastSpace > pos ? lastSpace : i));
        if (row >= skipRows && row - skipRows < maxRows) text(fb, xl + pad, y, s + pos, end - pos, font, color);
        pos = end;
        while (pos < len && s[pos] == ' ') ++pos;
        ++row;
    }
    return row;
}

void App::drawAddInfo(uint8_t *fb)
{
    using namespace draw;
    std::memset(fb, BLACK, static_cast<size_t>(kW) * kH);
    textCentered(fb, 120, 11, "ADD AN ACCOUNT", 0, TEAL_L);
    static const char kText[] =
        "ON THE PHONE: open the UNA app, this app's settings, and paste the long secret key (or the otpauth link) of the account into 'Secret key or link'. "
        "Save, then press R1 here. The field is emptied after it was read. "
        "OR copy a file import.txt (otpauth links, one per line) or import.json (an export of Aegis) into the folder of the app. "
        "Google Authenticator: its export link goes into import.txt. The file is erased after it was read.";
    const int rows = wrapRows(fb, kText, 30, 12, 0, mAboutPage * 12, 12, WHITE, 3);
    const int maxPage = rows > 12 ? (rows - 1) / 12 : 0;
    if (mAboutPage > maxPage) { mAboutPage = maxPage; }
    textCentered(fb, 120, 214, "R1 CHECK  L2 MORE  R2 BACK", 0, GRAY_D);
}

void App::drawAbout(uint8_t *fb)
{
    using namespace draw;
    std::memset(fb, BLACK, static_cast<size_t>(kW) * kH);
    textCentered(fb, 120, 11, "ABOUT", 0, TEAL_L);
    static const char kText[] =
        "Authenticator for one-time codes (TOTP, RFC 6238: SHA-1, SHA-256, SHA-512; 6 - 8 digits). The keys are kept in an encrypted file; the PIN opens it. "
        "A PIN is short: whoever can copy the file can try all PINs on a computer. Keep the watch with you, and use a PIN that is not your birthday. "
        "The code is right only when the time of the watch is: it takes it from the phone.";
    const int rows = wrapRows(fb, kText, 30, 12, 0, mAboutPage * 12, 12, WHITE, 3);
    const int maxPage = rows > 12 ? (rows - 1) / 12 : 0;
    if (mAboutPage > maxPage) mAboutPage = maxPage;
    textCentered(fb, 120, 214, "L1 / L2 PAGE  R2 BACK", 0, GRAY_D);
}

bool App::tick(uint32_t nowMs, uint8_t *fb)
{
    mNow = nowMs;
    if (mMode == Mode::Unlocking) {
        const Vault::Result r = mVault.step(mP, kUnlockStepMs);
        mDirty = true;
        if (r == Vault::Result::WrongPin) {
            ++mWrongTries;
            openPin(PinFor::Unlock);
        } else if (r == Vault::Result::Failed) {
            openMode(Mode::ResetAsk);
        } else if (r == Vault::Result::Done) {
            mPwHint = digitsOnly(mPin) ? 0 : 1; mPinLen = mPinN;                // what the wheel is made ready for the next time
            if (mChanging) {
                mChanging = false;
                mVault.stir(mP.nowMs(), mP.utc());
                if (mVault.save(mP)) showMessage("PIN CHANGED", "", "", Mode::Menu);
                else showMessage("COULD NOT SAVE", "THE NEW PIN", "", Mode::Menu);
            } else if (mPinFor == PinFor::NewAgain) {
                mVault.stir(mP.nowMs(), mP.utc());
                if (!mVault.save(mP)) showMessage("COULD NOT SAVE", "", "", Mode::Pin);
                else unlocked_();
            } else unlocked_();
            std::memset(mPin, 0, sizeof(mPin));
            mPinN = 0;
            if (mVault.unlocked()) saveSettings();
        }
    }
    updateLight(false);
    if ((mMode == Mode::Codes || mMode == Mode::All) && mVault.unlocked()) {
        const uint32_t u = clock();
        if (u != mLastUtc) { mLastUtc = u; mDirty = true; }
    }
    if (!mDirty) return false;
    mDirty = false;
    switch (mMode) {
        case Mode::Pin: drawPin(fb); break;
        case Mode::Unlocking: drawUnlocking(fb); break;
        case Mode::Codes: drawCodes(fb); break;
        case Mode::All: drawAll(fb); break;
        case Mode::Menu: drawList(fb, "MENU"); break;
        case Mode::RemoveList: drawList(fb, "REMOVE WHICH?"); break;
        case Mode::RemoveAsk: {
            const Account &a = mVault.at(mRemoveIndex);
            char l2[40];
            std::snprintf(l2, sizeof(l2), "%s", a.issuer[0] ? a.issuer : a.name);
            l2[22] = 0;
            drawBox(fb, "REMOVE THE ACCOUNT?", l2, "THE KEY IS ERASED FOR GOOD", "R1 REMOVE   R2 NO");
            break;
        }
        case Mode::AddInfo: drawAddInfo(fb); break;
        case Mode::About: drawAbout(fb); break;
        case Mode::ConfirmExit: drawBox(fb, "LEAVE THE AUTHENTICATOR?", "", "THE KEYS ARE LOCKED AGAIN", "R1 YES    R2 NO"); break;
        case Mode::ResetAsk: drawBox(fb, "THE VAULT IS DAMAGED", "ERASE IT AND START NEW?", "THE KEYS IN IT ARE LOST", "R1 ERASE    R2 EXIT"); break;
        case Mode::Message: drawBox(fb, mMessage[0], mMessage[1], mMessage[2], "R1 / R2: OK"); break;
    }
    return true;
}

} // namespace ta
