#include <ta/Account.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ta {

bool Account::sameKeyAs(const Account &o) const
{
    return secretLen == o.secretLen && alg == o.alg && digits == o.digits && period == o.period && std::memcmp(secret, o.secret, secretLen) == 0;
}

// ---- the codes ---------------------------------------------------------------------------------------------------------------------------------
uint32_t hotp(Alg alg, const uint8_t *key, size_t keyLen, uint64_t counter, int digits)
{
    uint8_t msg[8];
    for (int i = 7; i >= 0; --i) { msg[i] = static_cast<uint8_t>(counter); counter >>= 8; }
    uint8_t mac[64];
    const int n = hmac(alg, key, keyLen, msg, 8, mac);
    const int off = mac[n - 1] & 0x0F;
    const uint32_t bin = ((static_cast<uint32_t>(mac[off]) & 0x7F) << 24) | (static_cast<uint32_t>(mac[off + 1]) << 16) | (static_cast<uint32_t>(mac[off + 2]) << 8) | mac[off + 3];
    uint32_t mod = 1;
    for (int i = 0; i < digits; ++i) mod *= 10;
    return bin % mod;
}

void totp(const Account &a, uint64_t utc, char *out, size_t cap)
{
    const uint32_t period = a.period ? a.period : 30;
    const uint32_t v = hotp(a.alg, a.secret, a.secretLen, utc / period, a.digits);
    std::snprintf(out, cap, "%0*u", a.digits, static_cast<unsigned>(v));
}

int totpSecondsLeft(const Account &a, uint64_t utc)
{
    const uint32_t period = a.period ? a.period : 30;
    return static_cast<int>(period - utc % period);
}

// ---- codecs ------------------------------------------------------------------------------------------------------------------------------------
int base32Decode(const char *s, uint8_t *out, int cap)
{
    uint32_t acc = 0;
    int bits = 0, n = 0;
    for (; *s; ++s) {
        char c = *s;
        if (c == ' ' || c == '-' || c == '=' || c == '\t' || c == '\r' || c == '\n') continue;
        int v;
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= '2' && c <= '7') v = c - '2' + 26;
        else return -1;
        acc = (acc << 5) | static_cast<uint32_t>(v);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            if (n >= cap) return -1;
            out[n++] = static_cast<uint8_t>(acc >> bits);
        }
    }
    return n;
}

int base64Decode(const char *s, uint8_t *out, int cap)
{
    uint32_t acc = 0;
    int bits = 0, n = 0;
    for (; *s; ++s) {
        const char c = *s;
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+' || c == '-') v = 62;
        else if (c == '/' || c == '_') v = 63;
        else if (c == '=' || c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        else return -1;
        acc = (acc << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (n >= cap) return -1;
            out[n++] = static_cast<uint8_t>(acc >> bits);
        }
    }
    return n;
}

static int hexv(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int percentDecode(const char *s, char *out, int cap)
{
    int n = 0;
    for (; *s && n < cap - 1; ++s) {
        if (*s == '%' && hexv(s[1]) >= 0 && hexv(s[2]) >= 0) { out[n++] = static_cast<char>(hexv(s[1]) * 16 + hexv(s[2])); s += 2; }
        else if (*s == '+') out[n++] = ' ';
        else out[n++] = *s;
    }
    out[n] = 0;
    return n;
}

namespace {

// UTF-8 to ISO 8859-1 (what the font has): letters with no place there become '?'; at most cap - 1 bytes
void latin1(const char *src, char *dst, int cap)
{
    int o = 0;
    for (const char *p = src; *p && o < cap - 1;) {
        uint32_t cp = static_cast<uint8_t>(*p++);
        int more = 0;
        if (cp >= 0xF0) { cp &= 7; more = 3; } else if (cp >= 0xE0) { cp &= 15; more = 2; } else if (cp >= 0xC0) { cp &= 31; more = 1; }
        while (more-- > 0 && (static_cast<uint8_t>(*p) & 0xC0) == 0x80) cp = (cp << 6) | (static_cast<uint8_t>(*p++) & 0x3F);
        if (cp < 32) continue;
        dst[o++] = cp < 256 ? static_cast<char>(cp) : '?';
    }
    while (o > 0 && dst[o - 1] == ' ') --o;
    dst[o] = 0;
}

bool ieq(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + 32);
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y + 32);
        if (x != y) return false;
    }
    return !*a && !*b;
}

bool startsWith(const char *s, const char *prefix)
{
    for (; *prefix; ++s, ++prefix) {
        char x = *s, y = *prefix;
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + 32);
        if (x != y) return false;
    }
    return true;
}

bool algFromName(const char *s, Alg &a)
{
    if (ieq(s, "SHA1") || ieq(s, "SHA-1")) { a = Alg::Sha1; return true; }
    if (ieq(s, "SHA256") || ieq(s, "SHA-256")) { a = Alg::Sha256; return true; }
    if (ieq(s, "SHA512") || ieq(s, "SHA-512")) { a = Alg::Sha512; return true; }
    return false;
}

void blank(Account &a)
{
    std::memset(&a, 0, sizeof(a));
    a.alg = Alg::Sha1; a.digits = 6; a.period = 30;
}

void setIssuer(Account &a, const char *s) { latin1(s, a.issuer, kAccIssuer); }
void setName(Account &a, const char *s) { latin1(s, a.name, kAccName); }

bool setSecretBase32(Account &a, const char *s)
{
    const int n = base32Decode(s, a.secret, kSecretMax);
    if (n < 10) return false;                                            // a key of less than 80 bits is not one that a service gives out
    a.secretLen = static_cast<uint8_t>(n);
    return true;
}

} // namespace

// ---- otpauth://totp/Issuer:name?secret=..&issuer=..&algorithm=..&digits=..&period=.. -----------------------------------------------------------
int parseOtpauth(const char *uri, Account *out, int max)
{
    if (!startsWith(uri, "otpauth://")) return -1;
    if (max < 1) return 0;
    const char *p = uri + 10;
    if (!startsWith(p, "totp/") && !startsWith(p, "totp?")) return 0;       // hotp (a counter) and the rest are not codes of a clock
    p += 4;
    if (*p == '/') ++p;
    const char *q = std::strchr(p, '?');
    const size_t labelLen = q ? static_cast<size_t>(q - p) : std::strlen(p);
    char label[128];
    {
        char raw[128];
        const size_t n = labelLen < sizeof(raw) - 1 ? labelLen : sizeof(raw) - 1;
        std::memcpy(raw, p, n); raw[n] = 0;
        percentDecode(raw, label, sizeof(label));
    }
    Account a;
    blank(a);
    char labelIssuer[64] = {}, labelName[64] = {};
    if (const char *colon = std::strchr(label, ':')) {
        std::snprintf(labelIssuer, sizeof(labelIssuer), "%.*s", static_cast<int>(colon - label), label);
        const char *nm = colon + 1;
        while (*nm == ' ') ++nm;
        std::snprintf(labelName, sizeof(labelName), "%s", nm);
    } else std::snprintf(labelName, sizeof(labelName), "%s", label);
    bool haveSecret = false;
    char issuerParam[64] = {};
    int digits = 6, period = 30;
    Alg alg = Alg::Sha1;
    if (q) {
        for (const char *s = q + 1; *s;) {
            const char *amp = std::strchr(s, '&');
            const size_t len = amp ? static_cast<size_t>(amp - s) : std::strlen(s);
            char kv[256];
            const size_t n = len < sizeof(kv) - 1 ? len : sizeof(kv) - 1;
            std::memcpy(kv, s, n); kv[n] = 0;
            char *eq = std::strchr(kv, '=');
            if (eq) {
                *eq = 0;
                char val[200];
                percentDecode(eq + 1, val, sizeof(val));
                if (ieq(kv, "secret")) haveSecret = setSecretBase32(a, val);
                else if (ieq(kv, "issuer")) std::snprintf(issuerParam, sizeof(issuerParam), "%s", val);
                else if (ieq(kv, "algorithm")) { if (!algFromName(val, alg)) return 0; }
                else if (ieq(kv, "digits")) digits = std::atoi(val);
                else if (ieq(kv, "period")) period = std::atoi(val);
            }
            s += len;
            if (*s == '&') ++s;
        }
    }
    if (!haveSecret || digits < 6 || digits > 8 || period < 5 || period > 255) return 0;
    a.alg = alg; a.digits = static_cast<uint8_t>(digits); a.period = static_cast<uint8_t>(period);
    setIssuer(a, issuerParam[0] ? issuerParam : labelIssuer);
    setName(a, labelName);
    out[0] = a;
    return 1;
}

// ---- Google Authenticator: protobuf in base64 ----------------------------------------------------------------------------------------------------
namespace {

struct Pb {
    const uint8_t *p, *e;
    bool varint(uint64_t &v) { v = 0; for (int s = 0; p < e && s < 64; s += 7) { const uint8_t b = *p++; v |= static_cast<uint64_t>(b & 0x7F) << s; if (!(b & 0x80)) return true; } return false; }
    bool skip(uint32_t wire)
    {
        uint64_t v;
        switch (wire) {
            case 0: return varint(v);
            case 1: if (e - p < 8) return false; p += 8; return true;
            case 2: if (!varint(v) || static_cast<uint64_t>(e - p) < v) return false; p += v; return true;
            case 5: if (e - p < 4) return false; p += 4; return true;
            default: return false;
        }
    }
};

// one OtpParameters message
int migrationEntry(const uint8_t *p, size_t n, Account &a)
{
    Pb pb{ p, p + n };
    blank(a);
    char issuer[96] = {}, name[96] = {};
    uint64_t algo = 0, digitsV = 0, type = 2;
    bool secret = false;
    while (pb.p < pb.e) {
        uint64_t tag;
        if (!pb.varint(tag)) return -1;
        const uint32_t field = static_cast<uint32_t>(tag >> 3), wire = static_cast<uint32_t>(tag & 7);
        if (wire == 2) {
            uint64_t len;
            if (!pb.varint(len) || static_cast<uint64_t>(pb.e - pb.p) < len) return -1;
            if (field == 1) { if (len > static_cast<uint64_t>(kSecretMax) || len < 10) return 0; std::memcpy(a.secret, pb.p, len); a.secretLen = static_cast<uint8_t>(len); secret = true; }
            else if (field == 2) { const size_t c = len < sizeof(name) - 1 ? len : sizeof(name) - 1; std::memcpy(name, pb.p, c); name[c] = 0; }
            else if (field == 3) { const size_t c = len < sizeof(issuer) - 1 ? len : sizeof(issuer) - 1; std::memcpy(issuer, pb.p, c); issuer[c] = 0; }
            pb.p += len;
        } else if (wire == 0) {
            uint64_t v;
            if (!pb.varint(v)) return -1;
            if (field == 4) algo = v; else if (field == 5) digitsV = v; else if (field == 6) type = v;
        } else if (!pb.skip(wire)) return -1;
    }
    if (!secret || type == 1) return 0;                                  // no key, or a counter (HOTP)
    if (algo == 0 || algo == 1) a.alg = Alg::Sha1; else if (algo == 2) a.alg = Alg::Sha256; else if (algo == 3) a.alg = Alg::Sha512; else return 0;   // 4 is MD5
    a.digits = digitsV == 2 ? 8 : 6;
    char dec[96];
    // a name in the form "Issuer:account" when the issuer field has the same issuer
    const char *nm = name;
    if (const char *colon = std::strchr(name, ':')) {
        if (!issuer[0]) std::snprintf(issuer, sizeof(issuer), "%.*s", static_cast<int>(colon - name), name);
        nm = colon + 1;
        while (*nm == ' ') ++nm;
    }
    std::snprintf(dec, sizeof(dec), "%s", nm);
    setIssuer(a, issuer);
    setName(a, dec);
    return 1;
}

} // namespace

int parseMigration(const char *uri, Account *out, int max)
{
    if (!startsWith(uri, "otpauth-migration://")) return -1;
    const char *d = std::strstr(uri, "data=");
    if (!d) return 0;
    d += 5;
    const char *end = d;
    while (*end && *end != '&' && *end != ' ' && *end != '\r' && *end != '\n') ++end;
    static char enc[2048], dec[2048];
    const size_t n = static_cast<size_t>(end - d) < sizeof(enc) - 1 ? static_cast<size_t>(end - d) : sizeof(enc) - 1;
    std::memcpy(enc, d, n); enc[n] = 0;
    {                                                                    // %XX, but a '+' stays a '+' (it is part of the base64)
        int o = 0;
        for (size_t i = 0; enc[i] && o < static_cast<int>(sizeof(dec)) - 1; ++i) {
            if (enc[i] == '%' && hexv(enc[i + 1]) >= 0 && hexv(enc[i + 2]) >= 0) { dec[o++] = static_cast<char>(hexv(enc[i + 1]) * 16 + hexv(enc[i + 2])); i += 2; }
            else dec[o++] = enc[i];
        }
        dec[o] = 0;
    }
    static uint8_t raw[1536];
    const int rn = base64Decode(dec, raw, sizeof(raw));
    if (rn <= 0) return 0;
    Pb top{ raw, raw + rn };
    int count = 0;
    while (top.p < top.e) {
        uint64_t tag;
        if (!top.varint(tag)) break;
        const uint32_t field = static_cast<uint32_t>(tag >> 3), wire = static_cast<uint32_t>(tag & 7);
        if (field == 1 && wire == 2) {
            uint64_t len;
            if (!top.varint(len) || static_cast<uint64_t>(top.e - top.p) < len) break;
            Account a;
            if (count < max && migrationEntry(top.p, static_cast<size_t>(len), a) == 1) out[count++] = a;
            top.p += len;
        } else if (!top.skip(wire)) break;
    }
    return count;
}

// ---- Aegis: JSON ---------------------------------------------------------------------------------------------------------------------------------
namespace {

struct Json {
    const char *p, *e;
    void ws() { while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p; }
    bool str(char *out, int cap)                                         // p is at the opening quote
    {
        if (p >= e || *p != '"') return false;
        ++p;
        int o = 0;
        while (p < e && *p != '"') {
            char c = *p++;
            if (c == '\\' && p < e) {
                const char x = *p++;
                if (x == 'n' || x == 't' || x == 'r') c = ' ';
                else if (x == 'u' && e - p >= 4) {
                    unsigned v = 0;
                    for (int i = 0; i < 4; ++i) { const int h = hexv(p[i]); v = v * 16 + static_cast<unsigned>(h < 0 ? 0 : h); }
                    p += 4;
                    if (v < 0x80) c = static_cast<char>(v);
                    else if (v < 0x800) { if (out && o < cap - 2) { out[o++] = static_cast<char>(0xC0 | (v >> 6)); out[o++] = static_cast<char>(0x80 | (v & 0x3F)); } continue; }
                    else { if (out && o < cap - 3) { out[o++] = static_cast<char>(0xE0 | (v >> 12)); out[o++] = static_cast<char>(0x80 | ((v >> 6) & 0x3F)); out[o++] = static_cast<char>(0x80 | (v & 0x3F)); } continue; }
                } else c = x;
            }
            if (out && o < cap - 1) out[o++] = c;
        }
        if (p >= e) return false;
        ++p;
        if (out) out[o] = 0;
        return true;
    }
    bool skipValue()
    {
        ws();
        if (p >= e) return false;
        if (*p == '"') return str(nullptr, 0);
        if (*p == '{' || *p == '[') {
            const char open = *p, close = open == '{' ? '}' : ']';
            ++p;
            ws();
            if (p < e && *p == close) { ++p; return true; }
            for (;;) {
                if (open == '{') { ws(); if (!str(nullptr, 0)) return false; ws(); if (p >= e || *p != ':') return false; ++p; }
                if (!skipValue()) return false;
                ws();
                if (p < e && *p == ',') { ++p; continue; }
                if (p < e && *p == close) { ++p; return true; }
                return false;
            }
        }
        while (p < e && *p != ',' && *p != '}' && *p != ']') ++p;       // a number, true, false, null
        return true;
    }
};

bool jsonNumberOrString(Json &j, char *out, int cap)                      // the value as text (a number or a string)
{
    j.ws();
    if (j.p < j.e && *j.p == '"') return j.str(out, cap);
    int o = 0;
    while (j.p < j.e && *j.p != ',' && *j.p != '}' && *j.p != ']' && *j.p != ' ' && *j.p != '\n' && *j.p != '\r') { if (o < cap - 1) out[o++] = *j.p; ++j.p; }
    out[o] = 0;
    return true;
}

// one entry of "entries": { "type": "totp", "name": .., "issuer": .., "info": { "secret": .., "algo": .., "digits": .., "period": .. } }
int aegisEntry(Json &j, Account &a)
{
    blank(a);
    char type[16] = {}, name[96] = {}, issuer[96] = {}, secret[200] = {}, algo[16] = "SHA1", num[24];
    int digits = 6, period = 30;
    j.ws();
    if (j.p >= j.e || *j.p != '{') return j.skipValue() ? 0 : -1;
    ++j.p;
    j.ws();
    if (j.p < j.e && *j.p == '}') { ++j.p; return 0; }
    for (;;) {
        char key[32];
        j.ws();
        if (!j.str(key, sizeof(key))) return -1;
        j.ws();
        if (j.p >= j.e || *j.p != ':') return -1;
        ++j.p;
        j.ws();
        if (!std::strcmp(key, "type")) jsonNumberOrString(j, type, sizeof(type));
        else if (!std::strcmp(key, "name")) jsonNumberOrString(j, name, sizeof(name));
        else if (!std::strcmp(key, "issuer")) jsonNumberOrString(j, issuer, sizeof(issuer));
        else if (!std::strcmp(key, "info") && j.p < j.e && *j.p == '{') {
            ++j.p;
            j.ws();
            if (j.p < j.e && *j.p == '}') ++j.p;
            else for (;;) {
                char k2[32];
                j.ws();
                if (!j.str(k2, sizeof(k2))) return -1;
                j.ws();
                if (j.p >= j.e || *j.p != ':') return -1;
                ++j.p;
                j.ws();
                if (!std::strcmp(k2, "secret")) jsonNumberOrString(j, secret, sizeof(secret));
                else if (!std::strcmp(k2, "algo")) jsonNumberOrString(j, algo, sizeof(algo));
                else if (!std::strcmp(k2, "digits")) { jsonNumberOrString(j, num, sizeof(num)); digits = std::atoi(num); }
                else if (!std::strcmp(k2, "period")) { jsonNumberOrString(j, num, sizeof(num)); period = std::atoi(num); }
                else if (!j.skipValue()) return -1;
                j.ws();
                if (j.p < j.e && *j.p == ',') { ++j.p; continue; }
                if (j.p < j.e && *j.p == '}') { ++j.p; break; }
                return -1;
            }
        } else if (!j.skipValue()) return -1;
        j.ws();
        if (j.p < j.e && *j.p == ',') { ++j.p; continue; }
        if (j.p < j.e && *j.p == '}') { ++j.p; break; }
        return -1;
    }
    if (!ieq(type, "totp")) return 0;                                    // hotp, steam, yandex ...: not codes this app can make
    Alg al;
    if (!algFromName(algo, al) || digits < 6 || digits > 8 || period < 5 || period > 255 || !setSecretBase32(a, secret)) return 0;
    a.alg = al; a.digits = static_cast<uint8_t>(digits); a.period = static_cast<uint8_t>(period);
    setIssuer(a, issuer);
    setName(a, name);
    return 1;
}

} // namespace

int parseAegis(const char *json, int len, Account *out, int max)
{
    const char *s = json;
    const char *end = json + len;
    const char *entries = nullptr;
    for (const char *f = s; f + 9 <= end; ++f) if (!std::memcmp(f, "\"entries\"", 9)) { entries = f + 9; break; }
    if (!entries) return -1;
    Json j{ entries, end };
    j.ws();
    if (j.p >= j.e || *j.p != ':') return -1;
    ++j.p;
    j.ws();
    if (j.p >= j.e || *j.p != '[') return -1;                            // (an encrypted export has a string here)
    ++j.p;
    int count = 0;
    j.ws();
    if (j.p < j.e && *j.p == ']') return 0;
    for (;;) {
        Account a;
        const int r = aegisEntry(j, a);
        if (r < 0) break;
        if (r == 1 && count < max) out[count++] = a;
        j.ws();
        if (j.p < j.e && *j.p == ',') { ++j.p; continue; }
        break;
    }
    return count;
}

// ---- text of several kinds -------------------------------------------------------------------------------------------------------------------------
namespace {

bool plausibleSecret(const char *s)
{
    int n = 0;
    uint8_t tmp[kSecretMax];
    for (const char *p = s; *p; ++p) if (*p != ' ' && *p != '-') ++n;
    return n >= 16 && base32Decode(s, tmp, sizeof(tmp)) >= 10;
}

} // namespace

int parseText(const char *text, int len, Account *out, int max, int *skipped)
{
    int count = 0, bad = 0;
    const char *p = text, *end = text + len;
    if (len >= 3 && static_cast<uint8_t>(p[0]) == 0xEF && static_cast<uint8_t>(p[1]) == 0xBB && static_cast<uint8_t>(p[2]) == 0xBF) p += 3;
    const char *first = p;
    while (first < end && (*first == ' ' || *first == '\n' || *first == '\r' || *first == '\t')) ++first;
    if (first < end && *first == '{') {                                  // an export of Aegis
        const int n = parseAegis(first, static_cast<int>(end - first), out, max);
        if (skipped) *skipped = 0;
        return n;
    }
    static char line[2200];
    while (p < end) {
        const char *nl = p;
        while (nl < end && *nl != '\n') ++nl;
        size_t n = static_cast<size_t>(nl - p);
        if (n >= sizeof(line)) n = sizeof(line) - 1;
        std::memcpy(line, p, n);
        line[n] = 0;
        p = nl < end ? nl + 1 : end;
        char *a = line;
        while (*a == ' ' || *a == '\t') ++a;
        char *z = a + std::strlen(a);
        while (z > a && (z[-1] == ' ' || z[-1] == '\r' || z[-1] == '\t')) *--z = 0;
        if (!*a || *a == '#') continue;
        int r = -1;
        if (startsWith(a, "otpauth://")) { r = parseOtpauth(a, out + count, max - count); }
        else if (startsWith(a, "otpauth-migration://")) { r = parseMigration(a, out + count, max - count); }
        else {
            // "label, key" or "label<TAB>key" or "label key": after the last comma, semicolon or tab the key (it may be in groups of letters); else the last word
            char *sp = nullptr;
            for (char *c = a; *c; ++c) if (*c == ',' || *c == ';' || *c == '\t') sp = c;
            if (!sp) sp = std::strrchr(a, ' ');
            const char *secret = sp ? sp + 1 : a;
            if (plausibleSecret(secret)) {
                Account acc;
                blank(acc);
                if (setSecretBase32(acc, secret)) {
                    if (sp) { *sp = 0; char *lb = a + std::strlen(a); while (lb > a && (lb[-1] == ' ')) *--lb = 0; setName(acc, a); }
                    else std::snprintf(acc.name, sizeof(acc.name), "Account");
                    if (count < max) { out[count] = acc; r = 1; } else r = 0;
                }
            }
        }
        if (r > 0) count += r; else ++bad;
        if (count >= max) break;
    }
    if (skipped) *skipped = bad;
    return count;
}

int parseForm(const char *code, const char *name, const char *issuer, int digits, int period, const char *algo, Account &out)
{
    char c[160];
    std::snprintf(c, sizeof(c), "%s", code);
    char *a = c;
    while (*a == ' ') ++a;
    char *z = a + std::strlen(a);
    while (z > a && (z[-1] == ' ' || z[-1] == '\r' || z[-1] == '\n')) *--z = 0;
    if (!*a) return 0;
    Account acc;
    blank(acc);
    if (startsWith(a, "otpauth://")) {
        Account t[1];
        if (parseOtpauth(a, t, 1) != 1) return -1;
        acc = t[0];
    } else if (startsWith(a, "otpauth-migration://")) {
        Account t[1];
        if (parseMigration(a, t, 1) < 1) return -1;
        acc = t[0];
    } else {
        if (!setSecretBase32(acc, a)) return -1;
        Alg al = Alg::Sha1;
        if (algo && *algo && !algFromName(algo, al)) return -1;
        acc.alg = al;
        acc.digits = static_cast<uint8_t>(digits >= 6 && digits <= 8 ? digits : 6);
        acc.period = static_cast<uint8_t>(period >= 5 && period <= 255 ? period : 30);
    }
    if (issuer && *issuer) setIssuer(acc, issuer);
    if (name && *name) setName(acc, name);
    else if (!acc.name[0]) std::snprintf(acc.name, sizeof(acc.name), "Account");
    out = acc;
    return 1;
}

} // namespace ta
