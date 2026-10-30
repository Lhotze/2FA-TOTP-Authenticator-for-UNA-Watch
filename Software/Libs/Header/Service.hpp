#ifndef __SERVICE_HPP__
#define __SERVICE_HPP__

#include "SDK/Kernel/KernelProviderService.hpp"

#include <cstdint>

// The authenticator has little to do in the background: everything happens in the screen part. The Service follows the life of the app (and ends with it),
// keeps the screen lit when the user asks for it, and tells the screen the time (the codes are made from it).
class Service
{
public:
    Service(SDK::Kernel &kernel);
    virtual ~Service();

    void run();

private:
    void setBacklight(bool on, uint8_t percent);
    void publishClock();

    SDK::Kernel &mKernel;
    bool         mGuiStarted;
    bool         mBacklightSet;
};

#endif
