#include "Service.hpp"
#include "Commands.hpp"

#include "SDK/Messages/CommandMessages.hpp"
#include "SDK/Messages/MessageGuard.hpp"

#include <ctime>

#define LOG_MODULE_PRX      "Service"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

namespace {
constexpr uint32_t kNoGuiGraceMs = 15000;      // a Service that was started without a screen (the glances screen of an older kernel) ends after this
constexpr uint32_t kWaitMs = 1000;
constexpr uint32_t kClockMs = 1000;
constexpr uint32_t kBacklightDefaultMs = 5000;   // what the screen does again when the app lets go
constexpr uint32_t kBacklightOnMs = 20000;       // the screen is asked to stay lit for this long, and again before it is over
}

Service::Service(SDK::Kernel &kernel)
    : mKernel(SDK::KernelProviderService::GetInstance().getKernel())
    , mGuiStarted(false)
    , mBacklightSet(false)
{
    (void)kernel;
}

Service::~Service() {}

// on: lit at this brightness, for 20 s from now (the screen asks again every 5 s, so it stays lit, and it goes out by itself if the app is gone);
// off: the usual timeout again. The message has to be given back after it was sent, or the pool of messages runs dry after a while.
void Service::setBacklight(bool on, uint8_t percent)
{
    if (!on && !mBacklightSet) return;                  // never touched: leave the system's own setting alone
    auto *msg = mKernel.comm.allocateMessage<SDK::Message::RequestBacklightSet>();
    if (!msg) { LOG_INFO("No message for the backlight\n"); return; }
    msg->brightness = percent > 100 ? 100 : percent;
    msg->autoOffTimeoutMs = on ? kBacklightOnMs : kBacklightDefaultMs;
    mKernel.comm.sendMessage(msg);
    mKernel.comm.releaseMessage(msg);
    mBacklightSet = on;
}

// The time as the watch knows it (from the phone): seconds since 1970 in UTC, 0 when the clock was never set.
void Service::publishClock()
{
    if (!mGuiStarted) return;
    auto msg = SDK::make_msg<CustomMessage::ClockState>(mKernel);
    if (!msg) return;
    const std::time_t t = std::time(nullptr);
    msg->utc = t > 1600000000 ? static_cast<uint32_t>(t) : 0;
    msg.send();
}

void Service::run()
{
    LOG_INFO("thread started\n");
    const uint32_t startMs = mKernel.sys.getTimeMs();
    bool guiSeen = false;
    uint32_t lastClock = 0;

    while (true) {
        SDK::MessageBase *msg = nullptr;
        const bool got = mKernel.comm.getMessage(msg, kWaitMs);
        if (!guiSeen && !mGuiStarted && mKernel.sys.getTimeMs() - startMs > kNoGuiGraceMs) {   // started, but nobody ever came
            LOG_INFO("No screen came, ending\n");
            return;
        }
        if (mGuiStarted && mKernel.sys.getTimeMs() - lastClock >= kClockMs) { lastClock = mKernel.sys.getTimeMs(); publishClock(); }
        if (!got) continue;

        switch (msg->getType()) {
            case SDK::MessageType::COMMAND_APP_STOP:
                LOG_INFO("Force exit from the application\n");
                setBacklight(false, 100);
                mKernel.comm.releaseMessage(msg);
                return;

            case SDK::MessageType::COMMAND_APP_NOTIF_GUI_RUN:
                LOG_INFO("GUI is now running\n");
                mGuiStarted = true;
                guiSeen = true;
                lastClock = 0;
                break;

            case SDK::MessageType::EVENT_GLANCE_START:
                // an older kernel may ask every app for a glance: the authenticator is not one, so let go at once
                LOG_INFO("Not a glance, ending\n");
                mKernel.comm.releaseMessage(msg);
                return;

            case SDK::MessageType::COMMAND_APP_NOTIF_GUI_STOP:
                // The screen is gone: end the app.
                LOG_INFO("GUI has stopped\n");
                mGuiStarted = false;
                setBacklight(false, 100);
                SDK::send_msg<SDK::Message::RequestAppTerminate>(mKernel);
                break;

            case CustomMessage::BACKLIGHT_CONTROL: {
                const auto *b = static_cast<CustomMessage::BacklightControl *>(msg);
                setBacklight(b->on != 0, b->percent);
                } break;

            default:
                break;
        }

        mKernel.comm.releaseMessage(msg);
    }
}
