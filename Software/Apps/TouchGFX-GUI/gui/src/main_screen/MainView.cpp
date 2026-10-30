#include <gui/main_screen/MainView.hpp>
#include <touchgfx/Bitmap.hpp>

#include "SDK/Port/TouchGFX/TouchGFXCommandProcessor.hpp"

#include <cstring>

#if defined(SIMULATOR)
// Simulator only: WRISTAUTH_SIM_SCRIPT names a text file with one step per line,
//   <ticks to wait> k <key code>      press a button code (1-4 clicks, q/w/e/r press, a/s/d/f release)
//   <ticks to wait> shot <name>       the next screen update is written to the screenshots folder
#include <platform/hal/simulator/sdl2/HALSDL2.hpp>
#include <touchgfx/hal/HAL.hpp>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#define LOG_MODULE_PRX      "MainView"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"
namespace {
struct SimStep { uint32_t wait; bool isShot; char key; std::string name; };
std::vector<SimStep> gSimSteps;
size_t gSimPos = 0;
uint32_t gSimCountdown = 0;
bool gSimLoaded = false;
std::string gSimPendingShot;
uint32_t gSimPendingTicks = 0;
} // namespace
#endif


namespace {
alignas(16) uint8_t sAppStorage[sizeof(ta::App)];
}

MainView::MainView()
    : mApp(nullptr)
    , mExiting(false)
{
    std::memset(mBuf, 0xC0, sizeof(mBuf));             // opaque black
}

void MainView::setupScreen()
{
    MainViewBase::setupScreen();

    // the placeholders of the base screen are not used: one picture covers everything
    textArea1.setVisible(false);
    buttons.setVisible(false);

    mScreen.setPosition(0, 0, ta::draw::kW, ta::draw::kH);
    mScreen.setPixelData(mBuf);
    mScreen.setBitmapFormat(touchgfx::Bitmap::ABGR2222);
    add(mScreen);

    std::memset(mBuf, 0xC0, sizeof(mBuf));
    if (mApp) mApp->~App();
    mApp = new (sAppStorage) ta::App(presenter->platform());
    mApp->start();
    mApp->tick(presenter->nowMs(), mBuf);
    mScreen.invalidate();
}

void MainView::tearDownScreen()
{
    if (mApp) mApp->suspend(presenter->nowMs());
    MainViewBase::tearDownScreen();
}

void MainView::sleepNow()
{
    if (mApp) mApp->suspend(presenter->nowMs());
}

void MainView::handleKeyEvent(uint8_t key)
{
    if (mApp) mApp->onKey(key, presenter->nowMs());
}

#if defined(SIMULATOR)
void MainView::simScript()
{
    if (!gSimLoaded) {
        gSimLoaded = true;
        if (const char *path = std::getenv("WRISTAUTH_SIM_SCRIPT")) {
            std::ifstream in(path);
            std::string line;
            while (std::getline(in, line)) {
                if (line.empty() || line[0] == '#') continue;
                unsigned wait = 0;
                char kind[16] = {}, arg[64] = {};
                if (std::sscanf(line.c_str(), "%u %15s %63s", &wait, kind, arg) < 2) continue;
                SimStep st{ wait, false, 0, "" };
                if (std::strcmp(kind, "k") == 0) st.key = arg[0];
                else if (std::strcmp(kind, "shot") == 0) { st.isShot = true; st.name = arg; }
                else continue;
                gSimSteps.push_back(st);
            }
            if (!gSimSteps.empty()) gSimCountdown = gSimSteps[0].wait;
            LOG_INFO("SIMSCRIPT %u steps\n", static_cast<unsigned>(gSimSteps.size()));
        }
    }
    if (gSimPendingTicks > 0) {
        if (--gSimPendingTicks == 0) {
            static_cast<touchgfx::HALSDL2 *>(touchgfx::HAL::getInstance())->saveNextScreenshots(1);
            LOG_INFO("SIMSHOT %s\n", gSimPendingShot.c_str());
        }
        return;
    }
    if (gSimPos >= gSimSteps.size()) return;
    if (gSimCountdown > 0) { --gSimCountdown; return; }
    const SimStep &st = gSimSteps[gSimPos++];
    if (st.isShot) { gSimPendingShot = st.name; gSimPendingTicks = 3; }
    else handleKeyEvent(static_cast<uint8_t>(st.key));
    if (gSimPos < gSimSteps.size()) gSimCountdown = gSimSteps[gSimPos].wait;
}
#endif

void MainView::handleTickEvent()
{
    if (!mApp || mExiting) return;
#if defined(SIMULATOR)
    simScript();
#endif
    uint32_t ms = presenter->nowMs();

    // the system's key queue gives TouchGFX one code a frame, 10 frames a second: take all that are there
    uint8_t key = 0;
    while (SDK::TouchGFXCommandProcessor::GetInstance().getKeySample(key)) mApp->onKey(key, ms);

    if (mApp->tick(ms, mBuf)) mScreen.invalidate();

    if (mApp->wantsExit() && !mExiting) {
        mExiting = true;
        mApp->suspend(ms);
        presenter->exit();
    }
}
