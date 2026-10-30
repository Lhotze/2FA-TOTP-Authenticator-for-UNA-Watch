/**
 ******************************************************************************
 * @file    Account.hpp
 * @brief   One account of the authenticator, the one-time code of it, and the readers of the ways the keys come in (otpauth links, the export of Google
 *          Authenticator, the export of Aegis, plain text).
 ******************************************************************************
 */

#ifndef TA_ACCOUNT_HPP
#define TA_ACCOUNT_HPP

#include <ta/Sha.hpp>

#include <cstddef>
#include <cstdint>

namespace ta {

constexpr int kAccIssuer = 24;          // bytes with the NUL
constexpr int kAccName = 28;
constexpr int kSecretMax = 64;
constexpr int kMaxAccounts = 32;

struct Account {
    char     issuer[kAccIssuer];        // "GitHub"
    char     name[kAccName];            // "me@example.com"
    uint8_t  secret[kSecretMax];
    uint8_t  secretLen;
    Alg      alg;
    uint8_t  digits;                    // 6 .. 8
    uint8_t  period;                    // seconds
    bool sameKeyAs(const Account &o) const;
};

// ---- the codes
// The one-time password of RFC 4226 for a counter, as a number with `digits` digits.
uint32_t hotp(Alg alg, const uint8_t *key, size_t keyLen, uint64_t counter, int digits);
// The code of RFC 6238 at a time (seconds since 1970): "123456" (digits characters).
void totp(const Account &a, uint64_t utc, char *out, size_t cap);
// The number of seconds that this code still holds (1 .. period).
int totpSecondsLeft(const Account &a, uint64_t utc);

// ---- text codecs
int base32Decode(const char *s, uint8_t *out, int cap);                    // spaces, '-' and '=' are ignored, any case; bytes written, -1: a letter that is not Base32
int base64Decode(const char *s, uint8_t *out, int cap);                    // standard or URL alphabet, with or without '=', spaces ignored; -1: bad
int percentDecode(const char *s, char *out, int cap);                      // %XX and (in a query) nothing else; length written

// ---- the readers: each returns how many accounts it found (they are appended to out, up to max); -1 for text that is not of this kind
int parseOtpauth(const char *uri, Account *out, int max);                  // otpauth://totp/Issuer:name?secret=...&issuer=...&algorithm=SHA1&digits=6&period=30
int parseMigration(const char *uri, Account *out, int max);                // otpauth-migration://offline?data=... (Google Authenticator)
int parseAegis(const char *json, int len, Account *out, int max);          // the unencrypted export of Aegis
// Any text: a file with several lines of the kinds above, an Aegis export, or lines like "GitHub me@example.com JBSWY3DPEHPK3PXP". skipped: lines that could not be used.
int parseText(const char *text, int len, Account *out, int max, int *skipped);
// The values of the one-shot form of the phone app: code (a long code or a link), name, issuer, digits, period, algorithm. 1: an account, 0: nothing there, -1: not usable
int parseForm(const char *code, const char *name, const char *issuer, int digits, int period, const char *algo, Account &out);

} // namespace ta

#endif // TA_ACCOUNT_HPP
