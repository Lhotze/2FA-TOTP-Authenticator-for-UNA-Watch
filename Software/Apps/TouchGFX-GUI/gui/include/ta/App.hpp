/**
 ******************************************************************************
 * @file    App.hpp
 * @brief   The authenticator: PIN, the one-time codes, the list of accounts, adding and removing. No watch code, so it runs on the PC too.
 * @details Everywhere: L1 up, L2 down, R1 choose, R2 back. On the codes: L1 / L2 the account before / after, R1 the menu.
 ******************************************************************************
 */

#ifndef TA_APP_HPP
#define TA_APP_HPP

#include <ta/Platform.hpp>
#include <ta/Vault.hpp>

#include <cstddef>
#include <cstdint>

namespace ta {

class App {
public:
    explicit App(Platform &p);

    void start();
    void onKey(uint8_t key, uint32_t nowMs);              // SDK::GUI::Button codes (only the presses count)
    // Draws if something changed; returns true then. fb: 240 x 240 bytes, the same buffer every time.
    bool tick(uint32_t nowMs, uint8_t *fb);
    void suspend(uint32_t nowMs);                         // the screen goes away: the keys are wiped, the PIN is asked again
    bool wantsExit() const { return mExit; }

    enum class Mode : uint8_t { Pin, Unlocking, Codes, All, Menu, AddInfo, RemoveList, RemoveAsk, About, ConfirmExit, Message, ResetAsk };

    // for the tests and the pictures
    Mode mode() const { return mMode; }
    int  accountCount() const { return mVault.count(); }
    int  current() const { return mCur; }
    const Account &account(int i) { return mVault.at(i); }
    bool unlocked() const { return mVault.unlocked(); }
    int  wheelCursor() const { return mPinCursor; }
    int  wheelCount() const { return wheelSize(); }
    int  wheelIndexOfChar(char c) const;                   // the entry of the wheel (of the layer that is shown) that is this character, -1: none
    int  wheelIndexOfLabel(const char *label) const;       // "DEL", "OK", "ABC" ...: -1: none
    int  layer() const { return static_cast<int>(mLayer); }  // 0 digits, 1 small letters, 2 capitals, 3 digits and signs
    const char *pinTyped() const { return mPin; }
    int  pinLengthHint() const { return mPinLen; }
    bool passwordHint() const { return mPwHint != 0; }
    int  progress() const { return mVault.progress(); }
    const char *message(int line) const { return mMessage[line]; }
    int  lightSetting() const { return mLight; }
    int  menuSize() const { return mLarge; }
    int  listCursor() const { return mCursor; }
    int  menuIndexOf(const char *start);                   // the entry of the menu whose text begins like this, -1: none
    void currentCode(char *out, size_t cap);               // the code of the account on the screen now ("" if none / no time)

private:
    enum class PinFor : uint8_t { Unlock, New, NewAgain, ChangeNew, ChangeAgain };
    enum class Layer : uint8_t { Digits, Lower, Upper, Num };
    struct WheelEntry { char label[4]; uint8_t kind; char ch; };      // kind: 0 a character, 1 DEL, 2 OK, 3 to another layer (ch: 0 digits, 1 small, 2 capitals, 3 digits and signs)

    // ---- files
    void loadSettings();
    void saveSettings();
    void wipeFile(const char *name);
    // ---- the PIN
    void openPin(PinFor purpose);
    void pinKey(uint8_t key);
    int  wheelSize() const;
    WheelEntry wheelEntry(int i) const;
    void addChar(char c);
    void pinChosen();
    void drawPin(uint8_t *fb);
    void unlocked_();
    // ---- adding
    int  checkImports(bool automatic);                     // the form of the phone app and the import files; the number added, -1: a message is shown
    void addInfoText(char lines[][40], int &n);
    // ---- screens
    void drawCodes(uint8_t *fb);
    void drawAll(uint8_t *fb);
    void drawList(uint8_t *fb, const char *title);
    void drawUnlocking(uint8_t *fb);
    void drawAddInfo(uint8_t *fb);
    void drawAbout(uint8_t *fb);
    void drawBox(uint8_t *fb, const char *l1, const char *l2, const char *l3, const char *hint);
    void showMessage(const char *l1, const char *l2, const char *l3, Mode next);
    void openMode(Mode m);
    int  listCount() const;
    void label(int i, char *out, size_t cap);
    void listChoose();
    void listKey(uint8_t key);
    void updateLight(bool force);
    uint32_t clock();                                      // seconds since 1970 now, 0: not known
    static void group(const char *digits, char *out);      // "123456" -> "123 456"
    int  listRows() const { return mLarge ? 5 : 8; }
    int  listRowH() const { return mLarge ? 34 : 20; }
    int  listFont() const { return mLarge ? 3 : 1; }

    Platform &mP;
    Vault     mVault;
    Mode      mMode;
    bool      mExit, mDirty;
    uint32_t  mNow;
    // settings
    int       mLight, mLarge, mLast;
    bool      mLightOn;
    uint32_t  mLightMs;
    // the PIN
    PinFor    mPinFor;
    char      mPin[kPassMax + 1], mPinFirst[kPassMax + 1];
    int       mPinN, mPinCursor;
    Layer     mLayer;
    int       mPinLen, mPwHint;                            // what the last unlock showed: how many characters, and whether it was not digits only (the wheel is made ready for it; not secret enough to matter: the length)
    int       mWrongTries;
    bool      mChanging;                                   // Unlocking: after a new PIN the file is saved
    // the codes
    int       mCur, mTop, mCursor;
    uint32_t  mLastUtc;
    // messages
    char      mMessage[3][40];
    Mode      mMessageNext;
    int       mRemoveIndex;
    int       mAboutPage;
};

} // namespace ta

#endif // TA_APP_HPP
