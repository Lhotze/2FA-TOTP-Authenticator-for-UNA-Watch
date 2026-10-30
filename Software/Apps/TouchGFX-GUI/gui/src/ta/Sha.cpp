#include <ta/Sha.hpp>

#include <cstring>

namespace ta {

namespace {

inline uint32_t rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
inline uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
inline uint64_t ror64(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }
inline uint32_t be32(const uint8_t *p) { return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3]; }
inline uint64_t be64(const uint8_t *p) { return (static_cast<uint64_t>(be32(p)) << 32) | be32(p + 4); }
inline void put32(uint8_t *p, uint32_t v) { p[0] = static_cast<uint8_t>(v >> 24); p[1] = static_cast<uint8_t>(v >> 16); p[2] = static_cast<uint8_t>(v >> 8); p[3] = static_cast<uint8_t>(v); }
inline void put64(uint8_t *p, uint64_t v) { put32(p, static_cast<uint32_t>(v >> 32)); put32(p + 4, static_cast<uint32_t>(v)); }

const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

const uint64_t K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL, 0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL };

} // namespace

// ---- SHA-1 -----------------------------------------------------------------------------------------------------------------------------------
void Sha1::init()
{
    h[0] = 0x67452301; h[1] = 0xEFCDAB89; h[2] = 0x98BADCFE; h[3] = 0x10325476; h[4] = 0xC3D2E1F0;
    total = 0; fill = 0;
}

void Sha1::block(const uint8_t *p)
{
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) w[i] = be32(p + 4 * i);
    for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        const uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

void Sha1::update(const uint8_t *data, size_t n)
{
    total += n;
    while (n > 0) {
        const size_t take = (64 - static_cast<size_t>(fill)) < n ? (64 - static_cast<size_t>(fill)) : n;
        std::memcpy(buf + fill, data, take);
        fill += static_cast<int>(take); data += take; n -= take;
        if (fill == 64) { block(buf); fill = 0; }
    }
}

void Sha1::final(uint8_t out[20])
{
    const uint64_t bits = total * 8;
    uint8_t pad = 0x80;
    update(&pad, 1);
    pad = 0;
    while (fill != 56) update(&pad, 1);
    uint8_t len[8];
    put64(len, bits);
    update(len, 8);
    for (int i = 0; i < 5; ++i) put32(out + 4 * i, h[i]);
}

// ---- SHA-256 ---------------------------------------------------------------------------------------------------------------------------------
void Sha256::init()
{
    static const uint32_t iv[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    std::memcpy(h, iv, sizeof(h));
    total = 0; fill = 0;
}

void Sha256::block(const uint8_t *p)
{
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = be32(p + 4 * i);
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
        const uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = hh + S1 + ch + K256[i] + w[i];
        const uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
        const uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = S0 + mj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void Sha256::update(const uint8_t *data, size_t n)
{
    total += n;
    if (fill > 0) {
        const size_t take = (64 - static_cast<size_t>(fill)) < n ? (64 - static_cast<size_t>(fill)) : n;
        std::memcpy(buf + fill, data, take);
        fill += static_cast<int>(take); data += take; n -= take;
        if (fill == 64) { block(buf); fill = 0; }
    }
    while (n >= 64) { block(data); data += 64; n -= 64; }
    if (n > 0) { std::memcpy(buf, data, n); fill = static_cast<int>(n); }
}

void Sha256::final(uint8_t out[32])
{
    const uint64_t bits = total * 8;
    buf[fill++] = 0x80;
    if (fill > 56) { std::memset(buf + fill, 0, static_cast<size_t>(64 - fill)); block(buf); fill = 0; }
    std::memset(buf + fill, 0, static_cast<size_t>(56 - fill));
    put64(buf + 56, bits);
    block(buf);
    for (int i = 0; i < 8; ++i) put32(out + 4 * i, h[i]);
}

// ---- SHA-512 ---------------------------------------------------------------------------------------------------------------------------------
void Sha512::init()
{
    static const uint64_t iv[8] = { 0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
                                    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL };
    std::memcpy(h, iv, sizeof(h));
    total = 0; fill = 0;
}

void Sha512::block(const uint8_t *p)
{
    uint64_t w[80];
    for (int i = 0; i < 16; ++i) w[i] = be64(p + 8 * i);
    for (int i = 16; i < 80; ++i) {
        const uint64_t s0 = ror64(w[i - 15], 1) ^ ror64(w[i - 15], 8) ^ (w[i - 15] >> 7);
        const uint64_t s1 = ror64(w[i - 2], 19) ^ ror64(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint64_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 80; ++i) {
        const uint64_t S1 = ror64(e, 14) ^ ror64(e, 18) ^ ror64(e, 41);
        const uint64_t ch = (e & f) ^ (~e & g);
        const uint64_t t1 = hh + S1 + ch + K512[i] + w[i];
        const uint64_t S0 = ror64(a, 28) ^ ror64(a, 34) ^ ror64(a, 39);
        const uint64_t mj = (a & b) ^ (a & c) ^ (b & c);
        const uint64_t t2 = S0 + mj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void Sha512::update(const uint8_t *data, size_t n)
{
    total += n;
    while (n > 0) {
        const size_t take = (128 - static_cast<size_t>(fill)) < n ? (128 - static_cast<size_t>(fill)) : n;
        std::memcpy(buf + fill, data, take);
        fill += static_cast<int>(take); data += take; n -= take;
        if (fill == 128) { block(buf); fill = 0; }
    }
}

void Sha512::final(uint8_t out[64])
{
    const uint64_t bits = total * 8;
    buf[fill++] = 0x80;
    if (fill > 112) { std::memset(buf + fill, 0, static_cast<size_t>(128 - fill)); block(buf); fill = 0; }
    std::memset(buf + fill, 0, static_cast<size_t>(120 - fill));
    put64(buf + 120, bits);                                              // (the upper 64 bits of the 128-bit length are zero)
    block(buf);
    for (int i = 0; i < 8; ++i) put64(out + 8 * i, h[i]);
}

// ---- HMAC ------------------------------------------------------------------------------------------------------------------------------------
namespace {

template <typename H, int D, int B>
int hmacT(const uint8_t *key, size_t keyLen, const uint8_t *msg, size_t msgLen, uint8_t *out)
{
    uint8_t k[B] = {};
    if (keyLen > static_cast<size_t>(B)) { H h; h.update(key, keyLen); h.final(k); }
    else std::memcpy(k, key, keyLen);
    uint8_t pad[B];
    for (int i = 0; i < B; ++i) pad[i] = k[i] ^ 0x36;
    uint8_t inner[D];
    { H h; h.update(pad, B); h.update(msg, msgLen); h.final(inner); }
    for (int i = 0; i < B; ++i) pad[i] = k[i] ^ 0x5c;
    { H h; h.update(pad, B); h.update(inner, D); h.final(out); }
    wipe(k, sizeof(k)); wipe(pad, sizeof(pad));
    return D;
}

} // namespace

int hmac(Alg alg, const uint8_t *key, size_t keyLen, const uint8_t *msg, size_t msgLen, uint8_t *out)
{
    switch (alg) {
        case Alg::Sha1: return hmacT<Sha1, 20, 64>(key, keyLen, msg, msgLen, out);
        case Alg::Sha256: return hmacT<Sha256, 32, 64>(key, keyLen, msg, msgLen, out);
        default: return hmacT<Sha512, 64, 128>(key, keyLen, msg, msgLen, out);
    }
}

void HmacSha256::setKey(const uint8_t *key, size_t keyLen)
{
    uint8_t k[64] = {};
    if (keyLen > 64) { Sha256 h; h.update(key, keyLen); h.final(k); }
    else std::memcpy(k, key, keyLen);
    uint8_t pad[64];
    for (int i = 0; i < 64; ++i) pad[i] = k[i] ^ 0x36;
    mInner.init(); mInner.update(pad, 64);
    for (int i = 0; i < 64; ++i) pad[i] = k[i] ^ 0x5c;
    mOuter.init(); mOuter.update(pad, 64);
    wipe(k, sizeof(k)); wipe(pad, sizeof(pad));
}

void HmacSha256::mac(const uint8_t *msg, size_t n, uint8_t out[32]) const { mac2(msg, n, nullptr, 0, out); }

void HmacSha256::mac2(const uint8_t *a, size_t na, const uint8_t *b, size_t nb, uint8_t out[32]) const
{
    Sha256 in = mInner;
    in.update(a, na);
    if (nb) in.update(b, nb);
    uint8_t d[32];
    in.final(d);
    Sha256 o = mOuter;
    o.update(d, 32);
    o.final(out);
}

bool constantTimeEqual(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t d = 0;
    for (size_t i = 0; i < n; ++i) d |= static_cast<uint8_t>(a[i] ^ b[i]);
    return d == 0;
}

void wipe(void *p, size_t n)
{
    volatile uint8_t *q = static_cast<volatile uint8_t *>(p);
    for (size_t i = 0; i < n; ++i) q[i] = 0;
}

} // namespace ta
