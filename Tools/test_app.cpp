// PC test of the authenticator: the PIN, the first start, the form of the phone app, import files, removing, changing the PIN, locking.
// Build and run: Tools/run_test.sh
#include <ta/App.hpp>
#include "SDK/GUI/Button.hpp"
#include "disk_platform.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace B = SDK::GUI::Button;
using namespace ta;
static int fails = 0;
#define CHECK(cond, msg) do { if (cond) printf("  ok    %s\n", msg); else { printf("  FAIL  %s\n", msg); ++fails; } } while (0)

static uint8_t fb[240 * 240];
static uint32_t now = 1000;
static void press(App &a, uint8_t k, int times = 1) { for (int i = 0; i < times; ++i) { now += 120; a.onKey(k, now); a.tick(now, fb); } }
static void tickOnce(App &a) { now += 120; a.tick(now, fb); }
static bool sh(const std::string &cmd) { return system(cmd.c_str()) == 0; }
static void put(const std::string &path, const std::string &text) { std::ofstream(path, std::ios::binary) << text; }
static std::string slurp(const std::string &path) { std::ifstream in(path, std::ios::binary); std::stringstream ss; ss << in.rdbuf(); return ss.str(); }

// the wheel: layers (0 digits, 1 small letters, 2 capitals, 3 digits and signs) with entries that change the layer
static void wheelToIdx(App &a, int target) { const int n = a.wheelCount(); const int d = ((target - a.wheelCursor()) % n + n) % n; if (d <= n / 2) press(a, B::L2_PRESS, d); else press(a, B::L1_PRESS, n - d); }
static void wheelToLabel(App &a, const char *label) { wheelToIdx(a, a.wheelIndexOfLabel(label)); }
static void gotoLayer(App &a, int target)
{
    while (a.layer() != target) {
        const int cur = a.layer();
        const char *label = cur == 0 ? "ABC" : (cur == 1 ? (target == 2 ? "Aa" : "123") : (cur == 2 ? (target == 1 ? "aa" : "123") : (target == 0 ? "PIN" : "abc")));
        if (cur == 1 && target == 0) label = "123";
        if (cur == 2 && target == 0) label = "123";
        wheelToLabel(a, label); press(a, B::R1_PRESS);
    }
}
static void typeSecret(App &a, const char *secret)
{
    for (const char *c = secret; *c; ++c) {
        const bool digit = *c >= '0' && *c <= '9';
        const int need = digit ? (a.layer() == 0 ? 0 : 3) : (*c >= 'a' && *c <= 'z' ? 1 : (*c >= 'A' && *c <= 'Z' ? 2 : 3));
        gotoLayer(a, need);
        wheelToIdx(a, a.wheelIndexOfChar(*c)); press(a, B::R1_PRESS);
    }
}
static void typePin(App &a, const char *pin) { typeSecret(a, pin); wheelToLabel(a, "OK"); press(a, B::R1_PRESS); }
static void settle(App &a, int maxTicks = 5000) { for (int i = 0; i < maxTicks && a.mode() == App::Mode::Unlocking; ++i) tickOnce(a); }

// RFC 6238: the key "12345678901234567890" in Base32, SHA-1, 8 digits, at time 59: 94287082
static const char *kRfcKey = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
static const char *kOther = "JBSWY3DPEHPK3PXPJBSWY3DPEHPK3PXP";

int main()
{
    const std::string W = "/tmp/claude-1000/ta_flow";
    sh("rm -rf " + W + " && mkdir -p " + W);

    printf("the first start: a PIN is made\n");
    {
        DiskPlatform p; p.root = W; p.utcNow = 59;
        static App a(p);
        a.start();
        CHECK(a.mode() == App::Mode::Pin && !a.unlocked(), "no vault yet: the app asks for a new PIN");
        typePin(a, "47");
        CHECK(a.mode() == App::Mode::Pin && a.pinTyped()[0] == '4', "a PIN of two digits is not taken (still asking)");
        wheelToLabel(a, "DEL"); press(a, B::R1_PRESS, 1); press(a, B::R1_PRESS, 1);       // DEL DEL
        typePin(a, "4711");
        CHECK(a.mode() == App::Mode::Pin && a.pinTyped()[0] == 0, "then it asks for the PIN again");
        typePin(a, "4712");
        CHECK(a.mode() == App::Mode::Message, "another PIN the second time: they differ");
        press(a, B::R1_PRESS);
        typePin(a, "4711"); typePin(a, "4711");
        CHECK(a.mode() == App::Mode::Unlocking || a.mode() == App::Mode::Codes, "the same twice: the key is made (on the PC that takes less than one step)");
        settle(a);
        CHECK(a.mode() == App::Mode::Codes && a.unlocked() && a.accountCount() == 0, "done: the codes (no accounts yet)");
        CHECK(Vault::fileExists(p), "vault.dat is there");
    }

    printf("a wrong PIN, the right one\n");
    {
        DiskPlatform p; p.root = W; p.utcNow = 59;
        static App a(p);
        a.start();
        CHECK(a.mode() == App::Mode::Pin, "the next start asks for the PIN");
        typePin(a, "1234"); settle(a);
        CHECK(a.mode() == App::Mode::Pin && !a.unlocked(), "a wrong PIN: back to the PIN");
        typePin(a, "4711"); settle(a);
        CHECK(a.mode() == App::Mode::Codes && a.unlocked(), "the right PIN opens it");
    }

    printf("the form of the phone app\n");
    {
        DiskPlatform p; p.root = W; p.utcNow = 59;
        p.formHas = true;
        std::snprintf(p.form.code, sizeof(p.form.code), "%s", kRfcKey);
        std::snprintf(p.form.name, sizeof(p.form.name), "me@example.com");
        std::snprintf(p.form.issuer, sizeof(p.form.issuer), "ACME");
        p.form.digits = 8; p.form.period = 30; std::snprintf(p.form.algo, sizeof(p.form.algo), "SHA1");
        static App a(p);
        a.start();
        typePin(a, "4711"); settle(a);
        CHECK(a.mode() == App::Mode::Message && std::strcmp(a.message(0), "1 ACCOUNT ADDED") == 0, "the code of the form is added");
        CHECK(!p.formHas && p.formCleared == 1, "and the form is emptied");
        press(a, B::R1_PRESS);
        char code[16];
        a.currentCode(code, sizeof(code));
        CHECK(a.mode() == App::Mode::Codes && std::strcmp(code, "94287082") == 0, "the code at time 59 is 94287082 (RFC 6238)");
        CHECK(a.accountCount() == 1 && std::strcmp(a.account(0).issuer, "ACME") == 0 && std::strcmp(a.account(0).name, "me@example.com") == 0, "issuer and name are as typed");
    }
    {
        DiskPlatform p; p.root = W; p.utcNow = 59;
        p.formHas = true;
        std::snprintf(p.form.code, sizeof(p.form.code), "%s", kRfcKey);
        p.form.name[0] = 0; p.form.issuer[0] = 0; p.form.digits = 8; p.form.period = 30; std::snprintf(p.form.algo, sizeof(p.form.algo), "SHA1");
        static App a(p);
        a.start(); typePin(a, "4711"); settle(a);
        CHECK(a.mode() == App::Mode::Message && std::strcmp(a.message(0), "NOTHING ADDED") == 0 && std::strcmp(a.message(1), "1 ALREADY THERE") == 0, "the same key again is not added twice");
        CHECK(!p.formHas, "(the form is emptied all the same)");
    }
    {
        DiskPlatform p; p.root = W; p.utcNow = 59;
        p.formHas = true;
        std::snprintf(p.form.code, sizeof(p.form.code), "not a key");
        p.form.digits = 6; p.form.period = 30; std::snprintf(p.form.algo, sizeof(p.form.algo), "SHA1");
        static App a(p);
        a.start(); typePin(a, "4711"); settle(a);
        CHECK(a.mode() == App::Mode::Message && std::strcmp(a.message(2), "1 NOT USABLE") == 0 && !p.formHas, "something that is no key is refused, and emptied: it must not stay where it can be read");
    }

    printf("import files\n");
    {
        DiskPlatform p; p.root = W; p.utcNow = 1700000000;
        const std::string lines = std::string("otpauth://totp/Svc%3Aone?secret=") + kOther + "&issuer=Svc\n# comment\nMy Bank, " + kOther + "\n";
        put(W + "/import.txt", lines);
        put(W + "/import 2.json", "{\"db\":{\"entries\":[{\"type\":\"totp\",\"name\":\"anna\",\"issuer\":\"Mail\",\"info\":{\"secret\":\"" + std::string(kRfcKey) + "\",\"algo\":\"SHA256\",\"digits\":6,\"period\":30}}]}}");
        static App a(p);
        a.start(); typePin(a, "4711"); settle(a);
        CHECK(a.mode() == App::Mode::Message && std::strcmp(a.message(0), "2 ACCOUNTS ADDED") == 0, "two files: the new accounts are added (the key of the second line is the same as the first: one only)");
        CHECK(!p.exists("import.txt") && !p.exists("import 2.json"), "the files are removed");
        bool zeroed = false;
        for (auto &r : p.removed) if (r.first == "import.txt") { zeroed = r.second.size() == lines.size() && r.second.find_first_not_of('\0') == std::string::npos; }
        CHECK(zeroed, "and they were written over with zeros before");
        CHECK(slurp(W + "/vault.dat").find(kOther) == std::string::npos, "the vault does not hold the key in the clear");
        press(a, B::R1_PRESS);
        CHECK(a.accountCount() == 3, "3 accounts now");
    }
    {
        // a vault that cannot be saved: the keys stay where they were
        DiskPlatform p; p.root = W; p.utcNow = 1700000000; p.failTmp = true;
        put(W + "/import.txt", std::string("Other, ") + "KRUGKIDROVUWG2ZAMJZG653OEBTYZ===");
        p.formHas = true;
        std::snprintf(p.form.code, sizeof(p.form.code), "MFRGGZDFMZTWQ2LKNNWG23TPOBYXE43UOV3HO6DZPI======");
        p.form.digits = 6; p.form.period = 30; std::snprintf(p.form.algo, sizeof(p.form.algo), "SHA1");
        static App a(p);
        a.start(); typePin(a, "4711"); settle(a);
        CHECK(a.mode() == App::Mode::Message && std::strcmp(a.message(0), "COULD NOT SAVE") == 0, "a vault that cannot be written: a message");
        CHECK(p.exists("import.txt") && p.formHas, "nothing is erased then (the keys are not lost)");

    }
    sh("rm -f " + W + "/import.txt");

    printf("the codes, the time\n");
    {
        DiskPlatform p; p.root = W; p.utcNow = 1111111109;
        static App a(p);
        a.start(); typePin(a, "4711"); settle(a);
        char c1[16], c2[16];
        a.currentCode(c1, sizeof(c1));
        press(a, B::L2_PRESS);
        a.currentCode(c2, sizeof(c2));
        CHECK(std::strlen(c1) >= 6 && std::strcmp(c1, c2) != 0, "L2: the next account (another code)");
        p.utcNow = 0;
        a.currentCode(c1, sizeof(c1));
        tickOnce(a);
        CHECK(c1[0] == 0, "without the time there is no code");
    }

    printf("removing an account\n");
    {
        DiskPlatform p; p.root = W; p.utcNow = 1700000000;
        static App a(p);
        a.start(); typePin(a, "4711"); settle(a);
        CHECK(a.accountCount() == 3, "three accounts");
        press(a, B::R1_PRESS);                                         // the menu
        press(a, B::L2_PRESS, 2); press(a, B::R1_PRESS);               // REMOVE ACCOUNT
        CHECK(a.mode() == App::Mode::RemoveList, "the list of accounts");
        press(a, B::L2_PRESS); press(a, B::R1_PRESS);
        CHECK(a.mode() == App::Mode::RemoveAsk, "the question first");
        press(a, B::R2_PRESS);
        CHECK(a.mode() == App::Mode::RemoveList && a.accountCount() == 3, "R2: no");
        press(a, B::R1_PRESS); press(a, B::R1_PRESS);
        CHECK(a.accountCount() == 2, "R1: the account is gone");
        a.suspend(now);
        CHECK(!a.unlocked() && a.mode() == App::Mode::Pin, "when the screen goes the keys are wiped and the PIN is asked");
    }
    {
        DiskPlatform p; p.root = W; p.utcNow = 1700000000;
        static App a(p);
        a.start(); typePin(a, "4711"); settle(a);
        CHECK(a.accountCount() == 2, "it was saved: two accounts after a new start");
    }

    printf("changing the PIN\n");
    {
        DiskPlatform p; p.root = W; p.utcNow = 1700000000;
        static App a(p);
        a.start(); typePin(a, "4711"); settle(a);
        press(a, B::R1_PRESS); press(a, B::L2_PRESS, 3); press(a, B::R1_PRESS);   // CHANGE PIN
        CHECK(a.mode() == App::Mode::Pin, "the PIN screen");
        typePin(a, "2468"); typePin(a, "2468"); settle(a);
        CHECK(a.mode() == App::Mode::Message && std::strcmp(a.message(0), "PIN CHANGED") == 0, "PIN changed");
    }
    {
        DiskPlatform p; p.root = W; p.utcNow = 1700000000;
        static App a(p);
        a.start(); typePin(a, "4711"); settle(a);
        CHECK(!a.unlocked(), "the old PIN no longer works");
        typePin(a, "2468"); settle(a);
        CHECK(a.unlocked() && a.accountCount() == 2, "the new one does");
    }

    printf("a check that stopped on the last start\n");
    {
        DiskPlatform p; p.root = W; p.utcNow = 1700000000;
        put(W + "/check.lock", "1");
        p.formHas = true;
        std::snprintf(p.form.code, sizeof(p.form.code), "%s", kRfcKey);
        p.form.digits = 6; p.form.period = 30; std::snprintf(p.form.algo, sizeof(p.form.algo), "SHA1");
        static App a(p);
        a.start(); typePin(a, "2468"); settle(a);
        CHECK(a.mode() == App::Mode::Message && std::strcmp(a.message(0), "THE LAST CHECK STOPPED") == 0 && p.formHas, "the marker is there: the form is not read by itself (and is left alone)");
        CHECK(!p.exists("check.lock"), "the marker is removed: the next start is normal");
        press(a, B::R1_PRESS);
        press(a, B::R1_PRESS); press(a, B::L2_PRESS); press(a, B::R1_PRESS);        // menu, ADD ACCOUNT
        press(a, B::R1_PRESS);                                                       // check now
        CHECK(a.mode() == App::Mode::Message && std::strcmp(a.message(0), "1 ACCOUNT ADDED") == 0 && !p.formHas, "ADD ACCOUNT > R1 reads it when the user asks");
        CHECK(!p.exists("check.lock"), "and the marker is gone again");
    }

    printf("a PIN of six, the wheel goes to OK by itself\n");
    {
        const std::string W2 = "/tmp/claude-1000/ta_flow2";
        sh("rm -rf " + W2 + " && mkdir -p " + W2);
        DiskPlatform p; p.root = W2; p.utcNow = 59;
        static App a(p);
        a.start();
        typeSecret(a, "123456");
        CHECK(a.wheelCursor() == a.wheelIndexOfLabel("OK"), "a PIN that is made: after the sixth digit the cursor is on OK");
        wheelToIdx(a, a.wheelIndexOfChar('7')); press(a, B::R1_PRESS);
        CHECK(std::strlen(a.pinTyped()) == 6, "a seventh digit is not taken");
        press(a, B::L2_PRESS, 0);
        wheelToLabel(a, "OK"); press(a, B::R1_PRESS);
        typeSecret(a, "123456");
        CHECK(a.wheelCursor() == a.wheelIndexOfLabel("OK"), "and the same for the second time");
        press(a, B::R1_PRESS); settle(a);
        CHECK(a.unlocked() && a.mode() == App::Mode::Codes, "the PIN of six digits works");
        a.suspend(now);
        CHECK(a.pinLengthHint() == 6, "the length is remembered (settings.dat)");
        typeSecret(a, "123456");
        CHECK(a.wheelCursor() == a.wheelIndexOfLabel("OK"), "the next unlock: after the sixth digit the wheel is on OK");
        press(a, B::R1_PRESS); settle(a);
        CHECK(a.unlocked(), "unlocked");
    }

    printf("the PIN of four of before: it works as it did\n");
    {
        const std::string W4 = "/tmp/claude-1000/ta_flow4";
        sh("rm -rf " + W4 + " && mkdir -p " + W4);
        DiskPlatform p; p.root = W4; p.utcNow = 1700000000;
        {                                                                // a vault as the version before made it: PIN 4711, no settings file with a hint
            static Vault v;
            v.stir(1, 2);
            v.beginNew("4711");
            while (v.step(p, 50) == Vault::Result::Running) {}
            Account t[1];
            parseOtpauth((std::string("otpauth://totp/Old:me?secret=") + kRfcKey + "&issuer=Old").c_str(), t, 1);
            v.add(t[0]);
            v.save(p);
            v.lock();
        }
        static App a(p);
        a.start();
        CHECK(a.pinLengthHint() == 0 && a.layer() == 0, "no hint: the wheel of digits");
        typeSecret(a, "4711");
        CHECK(a.wheelCursor() != a.wheelIndexOfLabel("OK"), "(no jump yet: it does not know that the PIN has four)");
        wheelToLabel(a, "OK"); press(a, B::R1_PRESS); settle(a);
        CHECK(a.unlocked() && a.accountCount() == 1, "the PIN of four of the old vault opens it");
        a.suspend(now);
        typeSecret(a, "4711");
        CHECK(a.pinLengthHint() == 4 && a.wheelCursor() == a.wheelIndexOfLabel("OK"), "from then on the wheel goes to OK after the fourth digit");
        press(a, B::R1_PRESS); settle(a);
        CHECK(a.unlocked(), "unlocked");
    }

    printf("a password\n");
    {
        const std::string W3 = "/tmp/claude-1000/ta_flow3";
        sh("rm -rf " + W3 + " && mkdir -p " + W3);
        DiskPlatform p; p.root = W3; p.utcNow = 59;
        static App a(p);
        a.start();
        typePin(a, "Ab3");
        CHECK(a.mode() == App::Mode::Pin && std::strlen(a.pinTyped()) == 3, "a password of three characters is too short");
        wheelToLabel(a, "DEL"); press(a, B::R1_PRESS, 3);
        typePin(a, "Ab3-xY9");
        CHECK(a.mode() == App::Mode::Pin && a.layer() == 1 && a.pinTyped()[0] == 0, "the second time the wheel is on the letters again (the first was a password)");
        typePin(a, "Ab3-xY9"); settle(a);
        CHECK(a.unlocked(), "the password makes the vault (small letters, capitals, digits and a sign)");
        a.suspend(now);
        CHECK(a.passwordHint() && a.pinLengthHint() == 7 && a.layer() == 1, "the next start: the wheel of letters, the length 7 is known");
        typeSecret(a, "Ab3-xY9");
        CHECK(a.wheelCursor() == a.wheelIndexOfLabel("OK"), "after the seventh character the wheel is on OK");
        press(a, B::R1_PRESS); settle(a);
        CHECK(a.unlocked(), "unlocked with the password");
        a.suspend(now);
        typePin(a, "ab3-xY9"); settle(a);
        CHECK(!a.unlocked(), "one letter different (small, not capital): refused");
        gotoLayer(a, 0);
        typePin(a, "1234567"); settle(a);
        CHECK(!a.unlocked(), "digits do not open it");
        CHECK(slurp(W3 + "/vault.dat").find("Ab3-xY9") == std::string::npos, "the password is not in the file");
    }

    printf("a damaged vault\n");
    {
        sh("printf 'garbage garbage garbage garbage garbage garbage garbage garbage' > " + W + "/vault.dat");
        DiskPlatform p; p.root = W;
        static App a(p);
        a.start(); typePin(a, "2468"); settle(a);
        CHECK(a.mode() == App::Mode::ResetAsk, "it asks whether to erase the vault and start again (it does not do it by itself)");
        press(a, B::R1_PRESS);
        CHECK(a.mode() == App::Mode::Pin && !p.exists("vault.dat"), "R1: erased, a new PIN is asked for");
    }

    printf("%s\n", fails ? "SOME CHECKS FAILED" : "all checks passed");
    return fails != 0;
}
