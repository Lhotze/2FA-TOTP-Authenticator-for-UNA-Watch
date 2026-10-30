#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <gui/common/FrontendApplication.hpp>
#include "Commands.hpp"
#include "SDK/AppConfig/AppConfig.hpp"
#include <gui/model/AppConfigFields.hpp>
#include "SDK/Messages/MessageGuard.hpp"

#include "SDK/Kernel/KernelProviderGUI.hpp"
#include "SDK/Port/TouchGFX/TouchGFXCommandProcessor.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstring>

#define LOG_MODULE_PRX      "Model"
#define LOG_MODULE_LEVEL    LOG_LEVEL_DEBUG
#include "SDK/UnaLogger/Logger.h"

#if defined(SIMULATOR)
    #include "touchgfx/canvas_widget_renderer/CanvasWidgetRenderer.hpp"
    #ifdef _WIN32
    #include "Windows.h"
    #endif
    #include <chrono>
    #include <ctime>
#endif

// ---- the files of the watch, for the program ----------------------------------------------------------------------------------------------------
namespace {

class WatchFile : public ta::File
{
public:
    WatchFile(std::unique_ptr<SDK::Interface::IFile> f, bool writing) : mFile(std::move(f)), mWriting(writing), mPos(0), mOpen(false), mSize(0)
    {
        if (!mFile) return;
        mOpen = mFile->open(writing, writing);
        if (mOpen && !writing) mSize = static_cast<uint32_t>(mFile->size());
    }
    ~WatchFile() override
    {
        if (mFile && mOpen) {
            if (mWriting) mFile->flush();
            mFile->close();
        }
    }
    bool ok() const { return mOpen; }
    uint32_t size() override { return mWriting ? mPos : mSize; }
    bool read(uint32_t offset, void *dst, uint32_t n) override
    {
        if (!mOpen || mWriting) return false;
        if (offset != mPos && !mFile->seek(offset)) return false;
        mPos = offset;
        return take(dst, n);
    }
    bool readNext(void *dst, uint32_t n) override
    {
        if (!mOpen || mWriting) return false;
        return take(dst, n);
    }
    bool write(const void *src, uint32_t n) override
    {
        if (!mOpen || !mWriting) return false;
        size_t bw = 0;
        const bool ok = mFile->write(static_cast<const char *>(src), n, bw) && bw == n;
        mPos += static_cast<uint32_t>(bw);
        return ok;
    }

private:
    bool take(void *dst, uint32_t n)
    {
        size_t br = 0;
        const bool ok = mFile->read(static_cast<char *>(dst), n, br) && br == n;
        mPos += static_cast<uint32_t>(br);
        return ok;
    }

    std::unique_ptr<SDK::Interface::IFile> mFile;
    bool     mWriting;
    uint32_t mPos;
    bool     mOpen;
    uint32_t mSize;
};

bool hasExtension(const char *name, const char *ext)
{
    const size_t n = std::strlen(name), e = std::strlen(ext);
    if (n <= e) return false;
    for (size_t i = 0; i < e; ++i) {
        char c = name[n - e + i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c != ext[i]) return false;
    }
    return true;
}

int compareNames(const char *a, const char *b)
{
    for (;; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
        if (x != y || x == 0) return static_cast<unsigned char>(x) - static_cast<unsigned char>(y);
    }
}

} // namespace

class WatchPlatform : public ta::Platform
{
public:
    explicit WatchPlatform(Model &m, const SDK::Kernel &k) : mModel(m), mKernel(k) {}

    uint32_t nowMs() override { return mModel.nowMs(); }
    uint32_t utc() override { return mModel.utc(); }

    int listFiles(const char *ext, char names[][ta::kNameMax], int maxCount) override
    {
        int n = 0;
        std::unique_ptr<SDK::Interface::IDirectory> dir = mKernel.fs.dir("/");
        if (!dir || !dir->open()) return 0;
        SDK::Interface::IFileSystem::ObjectInfo item {};
        int seen = 0;
        while (n < maxCount && seen < 400 && dir->readNext(item)) {
            ++seen;
            if (item.isDir || item.isHidden || item.isSystem || item.name[0] == '.') continue;
            if (std::strlen(item.name) >= static_cast<size_t>(ta::kNameMax) || !hasExtension(item.name, ext)) continue;
            int at = n;                                    // insertion sort: the list is short
            while (at > 0 && compareNames(names[at - 1], item.name) > 0) { std::memcpy(names[at], names[at - 1], ta::kNameMax); --at; }
            std::snprintf(names[at], ta::kNameMax, "%s", item.name);
            ++n;
        }
        dir->close();
        return n;
    }

    bool remove(const char *name) override { return mKernel.fs.remove(name); }
    bool rename(const char *from, const char *to) override
    {
        if (mKernel.fs.exist(to)) mKernel.fs.remove(to);
        return mKernel.fs.rename(from, to);
    }

    ta::File *open(const char *name, bool forWriting) override
    {
        if (!forWriting && !mKernel.fs.exist(name)) return nullptr;
        auto *f = new WatchFile(mKernel.fs.file(name), forWriting);
        if (!f->ok()) { delete f; return nullptr; }
        return f;
    }

    bool exists(const char *name) override { return mKernel.fs.exist(name); }
    void backlight(bool alwaysOn, int percent) override { mModel.setBacklight(alwaysOn, percent); }
    void exitApp() override { mModel.exitApp(); }

    // The one-shot form of the phone app: the values of the configuration file (see app-manifest.json, configFields). They are read, and when the app
    // has them safe in its vault it calls clearForm(): the file is written over, then written again without them.
    bool readForm(ta::Form &f) override
    {
        mConfig.reset();                                               // a fresh read of the file
        mConfig.reset(new SDK::AppConfig(mKernel, kConfigFile, kConfigFields));
        std::memset(&f, 0, sizeof(f));
        mConfig->getString("newCode", f.code, sizeof(f.code));
        if (f.code[0] == 0) return false;
        mConfig->getString("newName", f.name, sizeof(f.name));
        mConfig->getString("newIssuer", f.issuer, sizeof(f.issuer));
        mConfig->getString("newAlgo", f.algo, sizeof(f.algo));
        f.digits = mConfig->getInt("newDigits");
        f.period = mConfig->getInt("newPeriod");
        return true;
    }

    void clearForm() override
    {
        if (!mConfig) mConfig.reset(new SDK::AppConfig(mKernel, kConfigFile, kConfigFields));
        uint32_t size = 0;
        if (mKernel.fs.exist(kConfigFile)) {
            std::unique_ptr<SDK::Interface::IFile> f = mKernel.fs.file(kConfigFile);
            if (f && f->open(false, false)) { size = static_cast<uint32_t>(f->size()); f->close(); }
        }
        if (size > 0) {                                                // what the file held is written over (the in-memory copy is what is written again)
            ta::File *w = open(kConfigFile, true);
            if (w) { uint8_t zeros[128] = {}; for (uint32_t done = 0; done < size; done += sizeof(zeros)) w->write(zeros, size - done < sizeof(zeros) ? size - done : static_cast<uint32_t>(sizeof(zeros))); delete w; }
        }
        static const char *const ids[] = { "newCode", "newName", "newIssuer", "newDigits", "newPeriod", "newAlgo" };
        for (const char *id : ids) mConfig->clear(id);
        mConfig->save();
        mConfig.reset();
    }

private:
    Model &mModel;
    const SDK::Kernel &mKernel;
    std::unique_ptr<SDK::AppConfig> mConfig;
};

Model::Model()
    : modelListener(0)
    , mKernel(SDK::KernelProviderGUI::GetInstance().getKernel())
    , mPlatform(new WatchPlatform(*this, mKernel))
{
    SDK::TouchGFXCommandProcessor::GetInstance().setAppLifeCycleCallback(this);
    SDK::TouchGFXCommandProcessor::GetInstance().setCustomMessageHandler(this);

#if defined(SIMULATOR)
    LOG_INFO("Application is running through simulator! \n");

    std::string fileStoreDir = SDK::Simulator::KernelHolder::Get().getFsPath();
    LOG_INFO("Path to files created by app (import.txt, app_config.json go here):\n"
        "       [%s]\n", fileStoreDir.c_str());

    LOG_INFO("\n"
        "---------------------------------------------------\n"
        "|   For Simulation Button use keybaord Keys.      |\n"
        "|       Keys Keybaord:                            |\n"
        "|       1   L1,                                   |\n"
        "|       2   L2,                                   |\n"
        "|       3   R1,                                   |\n"
        "|       4   R2                                    |\n"
        "---------------------------------------------------\n"
    );
#endif
}

Model::~Model() = default;

FrontendApplication& Model::application()
{
    return *static_cast<FrontendApplication*>(touchgfx::Application::getInstance());
}

void Model::tick()
{
    if (mInvalidate) {
        mInvalidate = false;
        application().invalidate();
    }
}

void Model::setBacklight(bool on, int percent)
{
    SDK::send_msg<CustomMessage::BacklightControl>(mKernel, static_cast<uint8_t>(on ? 1 : 0), static_cast<uint8_t>(percent < 0 ? 0 : (percent > 100 ? 100 : percent)));
}

void Model::exitApp()
{
    LOG_INFO("Manually exiting the application\n");

    SDK::TouchGFXCommandProcessor::GetInstance().setAppLifeCycleCallback(nullptr);
    SDK::TouchGFXCommandProcessor::GetInstance().setCustomMessageHandler(nullptr);

    mKernel.sys.exit(); // No return for real app
}

ta::Platform &Model::platform()
{
    return *mPlatform;
}

uint32_t Model::nowMs() const
{
    return mKernel.sys.getTimeMs();
}

uint32_t Model::utc() const
{
    if (mUtc == 0) return 0;
    return mUtc + (nowMs() - mUtcAtMs) / 1000;                         // between two messages the time runs on
}

bool Model::customMessageHandler(SDK::MessageBase *msg)
{
    if (msg && msg->getType() == CustomMessage::CLOCK_STATE) {
        const auto *m = static_cast<CustomMessage::ClockState *>(msg);
        mUtc = m->utc;
        mUtcAtMs = nowMs();
    }
    return true;
}

// IUserApp implementation
void Model::onStart()
{
    LOG_INFO("called\n");
}

void Model::onResume()
{
    LOG_INFO("called\n");
    mInvalidate = true;                     // redraw the screen
}

void Model::onStop()
{
    LOG_INFO("called\n");
    if (modelListener) modelListener->onSleep();
}

void Model::onSuspend()
{
    LOG_INFO("called\n");
    if (modelListener) modelListener->onSleep();
}
