// A driver for the tests of Tools/test_ta.py: it reads commands from stdin and prints what the code of the app makes of them.
//   T <alg 1|256|512> <digits> <period> <time> <base32 secret>              -> the code
//   P <hex of text>                                                          -> parseText: count, skipped, then one line per account
//   F <hex code> <hex name> <hex issuer> <digits> <period> <algo>             -> parseForm
// (hex of an empty string is written as "-")
#include <ta/Account.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace ta;
static std::string unhex(const char *s) { std::string r; if (!std::strcmp(s, "-")) return r; for (size_t i = 0; s[i] && s[i + 1]; i += 2) { char b[3] = { s[i], s[i + 1], 0 }; r.push_back(static_cast<char>(std::strtol(b, nullptr, 16))); } return r; }
static void show(const Account &a)
{
    std::printf("%s|%s|%d|%d|%d|", a.issuer, a.name, static_cast<int>(a.alg), a.digits, a.period);
    for (int i = 0; i < a.secretLen; ++i) std::printf("%02x", a.secret[i]);
    std::printf("\n");
}
int main()
{
    char cmd[8];
    static char big[40000];
    while (std::scanf("%7s", cmd) == 1) {
        if (!std::strcmp(cmd, "T")) {
            char alg[8], secret[256]; int digits, period; unsigned long long t;
            if (std::scanf("%7s %d %d %llu %255s", alg, &digits, &period, &t, secret) != 5) break;
            Account a; std::memset(&a, 0, sizeof(a));
            a.alg = !std::strcmp(alg, "1") ? Alg::Sha1 : (!std::strcmp(alg, "256") ? Alg::Sha256 : Alg::Sha512);
            a.digits = static_cast<uint8_t>(digits); a.period = static_cast<uint8_t>(period);
            a.secretLen = static_cast<uint8_t>(base32Decode(secret, a.secret, kSecretMax));
            char out[16]; totp(a, t, out, sizeof(out));
            std::printf("%s %d\n", out, totpSecondsLeft(a, t));
        } else if (!std::strcmp(cmd, "P")) {
            if (std::scanf("%39999s", big) != 1) break;
            std::string text = unhex(big);
            static Account acc[kMaxAccounts]; int skipped = 0;
            const int n = parseText(text.c_str(), static_cast<int>(text.size()), acc, kMaxAccounts, &skipped);
            std::printf("%d %d\n", n, skipped);
            for (int i = 0; i < n; ++i) show(acc[i]);
        } else if (!std::strcmp(cmd, "F")) {
            char c[400], nm[100], is[100], algo[20]; int digits, period;
            if (std::scanf("%399s %99s %99s %d %d %19s", c, nm, is, &digits, &period, algo) != 6) break;
            Account a; std::memset(&a, 0, sizeof(a));
            const std::string code = unhex(c), name = unhex(nm), issuer = unhex(is), al = unhex(algo);
            const int r = parseForm(code.c_str(), name.c_str(), issuer.c_str(), digits, period, al.c_str(), a);
            std::printf("%d\n", r);
            if (r == 1) show(a);
        }
    }
    return 0;
}
