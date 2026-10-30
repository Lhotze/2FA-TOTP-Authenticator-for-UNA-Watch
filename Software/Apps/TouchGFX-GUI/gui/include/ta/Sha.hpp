/**
 ******************************************************************************
 * @file    Sha.hpp
 * @brief   SHA-1, SHA-256 and SHA-512 and HMAC over them (FIPS 180-4, RFC 2104). Small and portable; no memory is allocated.
 ******************************************************************************
 */

#ifndef TA_SHA_HPP
#define TA_SHA_HPP

#include <cstddef>
#include <cstdint>

namespace ta {

enum class Alg : uint8_t { Sha1 = 0, Sha256 = 1, Sha512 = 2 };

constexpr int digestSize(Alg a) { return a == Alg::Sha1 ? 20 : (a == Alg::Sha256 ? 32 : 64); }
constexpr int blockSize(Alg a) { return a == Alg::Sha512 ? 128 : 64; }

class Sha1 {
public:
    Sha1() { init(); }
    void init();
    void update(const uint8_t *data, size_t n);
    void final(uint8_t out[20]);
private:
    void block(const uint8_t *p);
    uint32_t h[5];
    uint8_t buf[64];
    uint64_t total;
    int fill;
};

class Sha256 {
public:
    Sha256() { init(); }
    void init();
    void update(const uint8_t *data, size_t n);
    void final(uint8_t out[32]);
private:
    void block(const uint8_t *p);
    uint32_t h[8];
    uint8_t buf[64];
    uint64_t total;
    int fill;
};

class Sha512 {
public:
    Sha512() { init(); }
    void init();
    void update(const uint8_t *data, size_t n);
    void final(uint8_t out[64]);
private:
    void block(const uint8_t *p);
    uint64_t h[8];
    uint8_t buf[128];
    uint64_t total;
    int fill;
};

// HMAC of msg under key. out has digestSize(alg) bytes. Returns that size.
int hmac(Alg alg, const uint8_t *key, size_t keyLen, const uint8_t *msg, size_t msgLen, uint8_t *out);

// HMAC-SHA256 with the two hash states after the key blocks kept: for many calls under one key (PBKDF2).
class HmacSha256 {
public:
    void setKey(const uint8_t *key, size_t keyLen);
    void mac(const uint8_t *msg, size_t n, uint8_t out[32]) const;
    // two pieces of message
    void mac2(const uint8_t *a, size_t na, const uint8_t *b, size_t nb, uint8_t out[32]) const;
private:
    Sha256 mInner, mOuter;
};

// Equal for sure without leaking where they differ.
bool constantTimeEqual(const uint8_t *a, const uint8_t *b, size_t n);
void wipe(void *p, size_t n);

} // namespace ta

#endif // TA_SHA_HPP
