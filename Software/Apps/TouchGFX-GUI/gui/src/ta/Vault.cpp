#include <ta/Vault.hpp>

#include <cstring>

namespace ta {

namespace {

constexpr int kHeader = 42;            // magic 4, iterations 4, salt 16, nonce 16, length 2
constexpr uint32_t kMaxIterations = 2000000;

inline void put32le(uint8_t *p, uint32_t v) { p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8); p[2] = static_cast<uint8_t>(v >> 16); p[3] = static_cast<uint8_t>(v >> 24); }
inline uint32_t get32le(const uint8_t *p) { return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24); }

// the stream: block i = HMAC-SHA256(enc, nonce | i)
void crypt(const uint8_t enc[32], const uint8_t nonce[16], uint8_t *data, int n)
{
    HmacSha256 m;
    m.setKey(enc, 32);
    for (int off = 0, i = 0; off < n; off += 32, ++i) {
        const uint8_t idx[4] = { static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i) };
        uint8_t ks[32];
        m.mac2(nonce, 16, idx, 4, ks);
        for (int k = 0; k < 32 && off + k < n; ++k) data[off + k] ^= ks[k];
        wipe(ks, sizeof(ks));
    }
}

} // namespace

Vault::Vault()
    : mAcc{}, mCount(0), mUnlocked(false), mTask(Task::None), mU{}, mT{}, mIter(0), mDone(0), mSalt{}, mEncKey{}, mMacKey{}, mHaveKeys(false), mPool{}, mPoolCounter(0), mFile{}, mFileLen(0)
{
}

void Vault::stir(uint32_t a, uint32_t b)
{
    Sha256 h;
    h.update(mPool, 32);
    uint8_t x[12];
    put32le(x, a); put32le(x + 4, b); put32le(x + 8, ++mPoolCounter);
    h.update(x, sizeof(x));
    h.final(mPool);
}

void Vault::fresh(uint8_t *out, int n)
{
    for (int off = 0; off < n; off += 16) {
        stir(mPoolCounter * 2654435761u, static_cast<uint32_t>(off));
        uint8_t t[32];
        Sha256 h;
        h.update(mPool, 32);
        const uint8_t tag = 'F';
        h.update(&tag, 1);
        h.final(t);
        std::memcpy(out + off, t, static_cast<size_t>(n - off < 16 ? n - off : 16));
    }
}

void Vault::lock()
{
    wipe(mAcc, sizeof(mAcc));
    wipe(mEncKey, sizeof(mEncKey)); wipe(mMacKey, sizeof(mMacKey));
    wipe(mU, sizeof(mU)); wipe(mT, sizeof(mT));
    wipe(mFile, sizeof(mFile));
    mCount = 0; mUnlocked = false; mHaveKeys = false; mTask = Task::None;
}

// PBKDF2-HMAC-SHA256, one block of 32 bytes: U1 = HMAC(pin, salt | 00000001), T = U1 ^ U2 ^ ... ^ Uc
void Vault::startKdf(const char *pin, const uint8_t salt[16], uint32_t iterations)
{
    mKdfMac.setKey(reinterpret_cast<const uint8_t *>(pin), std::strlen(pin));
    static const uint8_t one[4] = { 0, 0, 0, 1 };
    mKdfMac.mac2(salt, 16, one, 4, mU);
    std::memcpy(mT, mU, 32);
    mIter = iterations;
    mDone = 1;
    std::memcpy(mSalt, salt, 16);
}

bool Vault::finishKdf()
{
    static const uint8_t encLabel[] = "ta-enc", macLabel[] = "ta-mac";
    HmacSha256 m;
    m.setKey(mT, 32);
    m.mac(encLabel, sizeof(encLabel) - 1, mEncKey);
    m.mac(macLabel, sizeof(macLabel) - 1, mMacKey);
    wipe(mT, sizeof(mT)); wipe(mU, sizeof(mU));
    mHaveKeys = true;
    return true;
}

int Vault::progress() const
{
    if (mTask == Task::None || mIter == 0) return 0;
    return static_cast<int>(static_cast<uint64_t>(mDone) * 100 / mIter);
}

bool Vault::beginUnlock(Platform &p, const char *pin)
{
    lock();
    File *f = p.open(kVaultFile, false);
    if (!f) return false;
    const uint32_t size = f->size();
    if (size < static_cast<uint32_t>(kHeader + 32) || size > sizeof(mFile) || !f->read(0, mFile, size)) { delete f; return false; }
    delete f;
    mFileLen = size;
    if (std::memcmp(mFile, "TAV1", 4) != 0) return false;
    const uint32_t iter = get32le(mFile + 4);
    const uint32_t len = static_cast<uint32_t>(mFile[40]) | (static_cast<uint32_t>(mFile[41]) << 8);
    if (iter < 1 || iter > kMaxIterations || size != static_cast<uint32_t>(kHeader) + len + 32) return false;
    startKdf(pin, mFile + 8, iter);
    mTask = Task::Unlock;
    return true;
}

void Vault::beginNew(const char *pin)
{
    lock();
    uint8_t salt[16];
    fresh(salt, 16);
    startKdf(pin, salt, kIterations);
    mTask = Task::New;
}

void Vault::beginPinChange(const char *pin)
{
    uint8_t salt[16];
    fresh(salt, 16);
    startKdf(pin, salt, kIterations);
    mTask = Task::PinChange;
}

Vault::Result Vault::step(Platform &p, uint32_t budgetMs)
{
    if (mTask == Task::None) return Result::Failed;
    const uint32_t t0 = p.nowMs();
    while (mDone < mIter) {
        for (int i = 0; i < 64 && mDone < mIter; ++i) {
            mKdfMac.mac(mU, 32, mU);
            for (int k = 0; k < 32; ++k) mT[k] ^= mU[k];
            ++mDone;
        }
        if (p.nowMs() - t0 >= budgetMs) break;
    }
    if (mDone < mIter) return Result::Running;
    finishKdf();
    const Task task = mTask;
    mTask = Task::None;
    if (task == Task::Unlock) {
        if (!decrypt()) { wipe(mEncKey, sizeof(mEncKey)); wipe(mMacKey, sizeof(mMacKey)); mHaveKeys = false; return Result::WrongPin; }
        mUnlocked = true;
        return Result::Done;
    }
    if (task == Task::New) { mCount = 0; mUnlocked = true; return Result::Done; }
    return Result::Done;                                                // a new key: the accounts are still there
}

bool Vault::decrypt()
{
    const uint32_t len = static_cast<uint32_t>(mFile[40]) | (static_cast<uint32_t>(mFile[41]) << 8);
    uint8_t tag[32];
    HmacSha256 m;
    m.setKey(mMacKey, 32);
    m.mac(mFile, kHeader + len, tag);
    if (!constantTimeEqual(tag, mFile + kHeader + len, 32)) return false;
    static uint8_t plain[kMaxAccounts * 120 + 8];
    if (len > sizeof(plain)) return false;
    std::memcpy(plain, mFile + kHeader, len);
    crypt(mEncKey, mFile + 24, plain, static_cast<int>(len));
    const bool ok = deserialize(plain, static_cast<int>(len));
    wipe(plain, sizeof(plain));
    wipe(mFile, sizeof(mFile));
    return ok;
}

int Vault::serialize(uint8_t *out, int cap) const
{
    int o = 0;
    auto need = [&](int n) { return o + n <= cap; };
    if (!need(5)) return -1;
    std::memcpy(out, "TAv1", 4); o = 4;
    out[o++] = static_cast<uint8_t>(mCount);
    for (int i = 0; i < mCount; ++i) {
        const Account &a = mAcc[i];
        const int il = static_cast<int>(std::strlen(a.issuer)), nl = static_cast<int>(std::strlen(a.name));
        if (!need(1 + il + 1 + nl + 4 + a.secretLen)) return -1;
        out[o++] = static_cast<uint8_t>(il); std::memcpy(out + o, a.issuer, static_cast<size_t>(il)); o += il;
        out[o++] = static_cast<uint8_t>(nl); std::memcpy(out + o, a.name, static_cast<size_t>(nl)); o += nl;
        out[o++] = static_cast<uint8_t>(a.alg); out[o++] = a.digits; out[o++] = a.period; out[o++] = a.secretLen;
        std::memcpy(out + o, a.secret, a.secretLen); o += a.secretLen;
    }
    return o;
}

bool Vault::deserialize(const uint8_t *in, int n)
{
    if (n < 5 || std::memcmp(in, "TAv1", 4) != 0) return false;
    const int count = in[4];
    if (count > kMaxAccounts) return false;
    int o = 5;
    wipe(mAcc, sizeof(mAcc));
    for (int i = 0; i < count; ++i) {
        Account &a = mAcc[i];
        if (o >= n) return false;
        const int il = in[o++];
        if (il >= kAccIssuer || o + il + 1 > n) return false;
        std::memcpy(a.issuer, in + o, static_cast<size_t>(il)); o += il;
        const int nl = in[o++];
        if (nl >= kAccName || o + nl + 4 > n) return false;
        std::memcpy(a.name, in + o, static_cast<size_t>(nl)); o += nl;
        a.alg = static_cast<Alg>(in[o++]); a.digits = in[o++]; a.period = in[o++];
        const int sl = in[o++];
        if (sl > kSecretMax || o + sl > n || static_cast<int>(a.alg) > 2 || a.digits < 6 || a.digits > 8 || a.period == 0) return false;
        a.secretLen = static_cast<uint8_t>(sl);
        std::memcpy(a.secret, in + o, static_cast<size_t>(sl)); o += sl;
    }
    mCount = count;
    return true;
}

bool Vault::save(Platform &p)
{
    if (!mUnlocked || !mHaveKeys) return false;
    static uint8_t buf[kHeader + kMaxAccounts * 120 + 8 + 32];
    const int len = serialize(buf + kHeader, static_cast<int>(sizeof(buf)) - kHeader - 32);
    if (len < 0) return false;
    std::memcpy(buf, "TAV1", 4);
    put32le(buf + 4, mIter ? mIter : kIterations);
    std::memcpy(buf + 8, mSalt, 16);
    fresh(buf + 24, 16);                                                // a new nonce for every file: the same stream is never used twice
    buf[40] = static_cast<uint8_t>(len); buf[41] = static_cast<uint8_t>(len >> 8);
    crypt(mEncKey, buf + 24, buf + kHeader, len);
    HmacSha256 m;
    m.setKey(mMacKey, 32);
    m.mac(buf, static_cast<size_t>(kHeader + len), buf + kHeader + len);
    const uint32_t total = static_cast<uint32_t>(kHeader + len + 32);
    File *f = p.open("vault.tmp", true);
    if (!f) { wipe(buf, sizeof(buf)); return false; }
    const bool ok = f->write(buf, total);
    delete f;
    wipe(buf, sizeof(buf));
    if (!ok) { p.remove("vault.tmp"); return false; }
    if (!p.rename("vault.tmp", kVaultFile)) { p.remove("vault.tmp"); return false; }
    return true;
}

bool Vault::add(const Account &a)
{
    if (mCount >= kMaxAccounts) return false;
    for (int i = 0; i < mCount; ++i) if (mAcc[i].sameKeyAs(a)) return false;
    mAcc[mCount++] = a;
    return true;
}

void Vault::removeAt(int i)
{
    if (i < 0 || i >= mCount) return;
    for (int k = i; k + 1 < mCount; ++k) mAcc[k] = mAcc[k + 1];
    wipe(&mAcc[mCount - 1], sizeof(Account));
    --mCount;
}

} // namespace ta
