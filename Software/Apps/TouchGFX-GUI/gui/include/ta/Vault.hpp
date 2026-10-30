/**
 ******************************************************************************
 * @file    Vault.hpp
 * @brief   The accounts, kept in a file that is encrypted with a PIN.
 * @details File `vault.dat`:  "TAV1" | iterations u32 | salt 16 | nonce 16 | length u16 | ciphertext | tag 32   (all numbers little endian)
 *          The key comes from the PIN with PBKDF2-HMAC-SHA256 (salt, `iterations`), 32 bytes; from those two keys with HMAC-SHA256:
 *          enc = HMAC(master, "ta-enc"), mac = HMAC(master, "ta-mac"). The text is XORed with a stream: block i = HMAC-SHA256(enc, nonce | i as 4 bytes
 *          big endian). The tag is HMAC-SHA256(mac, everything in front of it). A wrong PIN shows as a wrong tag.
 *          This is made of the usual parts (not a new method of its own), but it is not a reviewed implementation. A PIN is short: whoever has the
 *          file can try all PINs on a PC. It keeps the keys from being read from the file by someone who does not take that trouble.
 ******************************************************************************
 */

#ifndef TA_VAULT_HPP
#define TA_VAULT_HPP

#include <ta/Account.hpp>
#include <ta/Platform.hpp>

namespace ta {

constexpr const char *kVaultFile = "vault.dat";
constexpr uint32_t kIterations = 20000;
constexpr int kPinMin = 4;            // a PIN (digits only): 4 to 6 digits when it is made; the key may also be a password of 6 to 24 characters
constexpr int kPinMaxNew = 6;
constexpr int kPassMin = 6, kPassMax = 24;

class Vault {
public:
    enum class Result : uint8_t { Running, Done, WrongPin, Failed };

    Vault();
    ~Vault() { lock(); }

    static bool fileExists(Platform &p) { return p.exists(kVaultFile); }

    // Each of these starts work that step() carries on in small pieces (the key of a PIN takes a while).
    bool beginUnlock(Platform &p, const char *pin);                    // false: no usable file
    void beginNew(const char *pin);                                    // a new empty vault with this PIN (not written until save())
    void beginPinChange(const char *pin);                              // the accounts stay, the key is a new one (save() after it)
    Result step(Platform &p, uint32_t budgetMs);
    int  progress() const;                                             // 0 .. 100

    bool unlocked() const { return mUnlocked; }
    bool save(Platform &p);                                            // writes the file (to a temporary file first)
    void lock();                                                       // the keys and the accounts are wiped

    int  count() const { return mCount; }
    Account &at(int i) { return mAcc[i]; }
    bool add(const Account &a);                                        // false: full, or the same key is there
    void removeAt(int i);
    void stir(uint32_t a, uint32_t b);                                 // the clock and the like go into what the salts and nonces are made of

private:
    enum class Task : uint8_t { None, Unlock, New, PinChange };
    void fresh(uint8_t *out, int n);
    void startKdf(const char *pin, const uint8_t salt[16], uint32_t iterations);
    bool finishKdf();                                                  // keys from the result
    bool decrypt();
    int  serialize(uint8_t *out, int cap) const;
    bool deserialize(const uint8_t *in, int n);

    Account  mAcc[kMaxAccounts];
    int      mCount;
    bool     mUnlocked;
    Task     mTask;
    // the key work
    HmacSha256 mKdfMac;
    uint8_t  mU[32], mT[32];
    uint32_t mIter, mDone;
    uint8_t  mSalt[16];
    uint8_t  mEncKey[32], mMacKey[32];
    bool     mHaveKeys;
    uint8_t  mPool[32];
    uint32_t mPoolCounter;
    // the file being unlocked
    uint8_t  mFile[64 + 4 + kMaxAccounts * 120 + 40];
    uint32_t mFileLen;
};

} // namespace ta

#endif // TA_VAULT_HPP
