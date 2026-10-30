#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include "SDK/Messages/MessageBase.hpp"
#include "SDK/Messages/MessageTypes.hpp"
#include "SDK/Messages/CommandMessages.hpp"

// Force 4-byte alignment for all message structures
#pragma pack(push, 4)

namespace CustomMessage {

    // The Service knows the time (the screen's own clock is not reliable). About once a second it pushes the time as seconds since 1970 (UTC), 0 if not set.
    constexpr SDK::MessageType::Type CLOCK_STATE = 0x00000010; // Service --> GUI

    struct ClockState : public SDK::MessageBase {
        uint32_t utc;                       // seconds since 1970, 0 if not set

        ClockState() : SDK::MessageBase(CLOCK_STATE), utc(0) {}
    };

    // GUI --> Service: the screen stays lit (on = 1, at this brightness in percent) or goes back to the usual timeout (on = 0).
    // Used when the user wants the screen lit all the time.
    constexpr SDK::MessageType::Type BACKLIGHT_CONTROL = 0x00000013;
    struct BacklightControl : public SDK::MessageBase {
        uint8_t on;
        uint8_t percent;
        BacklightControl() : SDK::MessageBase(BACKLIGHT_CONTROL), on(0), percent(100) {}
        BacklightControl(uint8_t o, uint8_t p) : SDK::MessageBase(BACKLIGHT_CONTROL), on(o), percent(p) {}
    };

} // namespace CustomMessage

#pragma pack(pop)

#endif // COMMANDS_HPP
