// A driver for the tests of Tools/test_vault.py.
//   NEW <folder> <pin> <hex of text with the accounts>   -> a vault.dat in the folder
//   UNLOCK <folder> <pin>                                  -> OK n and a line per account, or WRONG / FAIL
//   CHANGE <folder> <oldpin> <newpin>                      -> unlock, new key, save
#include "disk_platform.hpp"
#include <ta/Vault.hpp>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
using namespace ta;
static std::string unhex(const char *s) { std::string r; if (!std::strcmp(s, "-")) return r; for (size_t i = 0; s[i] && s[i + 1]; i += 2) { char b[3] = { s[i], s[i + 1], 0 }; r.push_back(static_cast<char>(std::strtol(b, nullptr, 16))); } return r; }
static ta::Vault::Result run(Vault &v, Platform &p) { Vault::Result r; do { r = v.step(p, 50); } while (r == Vault::Result::Running); return r; }
static void show(const Account &a) { std::printf("%s|%s|%d|%d|%d|", a.issuer, a.name, static_cast<int>(a.alg), a.digits, a.period); for (int i = 0; i < a.secretLen; ++i) std::printf("%02x", a.secret[i]); std::printf("\n"); }
int main()
{
    char cmd[16], folder[200], pin[40], pin2[40], text[40000];
    static Vault v;
    while (std::scanf("%15s", cmd) == 1) {
        DiskPlatform p;
        if (!std::strcmp(cmd, "NEW")) {
            if (std::scanf("%199s %39s %39999s", folder, pin, text) != 3) break;
            p.root = folder;
            v.stir(static_cast<uint32_t>(time(nullptr)), static_cast<uint32_t>(clock()) ^ static_cast<uint32_t>(getpid()));
            v.beginNew(pin);
            run(v, p);
            const std::string t = unhex(text);
            static Account acc[kMaxAccounts]; int skipped = 0;
            const int n = parseText(t.c_str(), static_cast<int>(t.size()), acc, kMaxAccounts, &skipped);
            for (int i = 0; i < n; ++i) v.add(acc[i]);
            std::printf("%s %d\n", v.save(p) ? "SAVED" : "SAVEFAIL", v.count());
            v.lock();
        } else if (!std::strcmp(cmd, "UNLOCK")) {
            if (std::scanf("%199s %39s", folder, pin) != 2) break;
            p.root = folder;
            if (!v.beginUnlock(p, pin)) { std::printf("FAIL\n"); continue; }
            const Vault::Result r = run(v, p);
            if (r != Vault::Result::Done) { std::printf("WRONG\n"); v.lock(); continue; }
            std::printf("OK %d\n", v.count());
            for (int i = 0; i < v.count(); ++i) show(v.at(i));
            v.lock();
        } else if (!std::strcmp(cmd, "CHANGE")) {
            if (std::scanf("%199s %39s %39s", folder, pin, pin2) != 3) break;
            p.root = folder;
            if (!v.beginUnlock(p, pin) || run(v, p) != Vault::Result::Done) { std::printf("WRONG\n"); v.lock(); continue; }
            v.beginPinChange(pin2);
            run(v, p);
            std::printf("%s\n", v.save(p) ? "CHANGED" : "SAVEFAIL");
            v.lock();
        }
    }
    return 0;
}
