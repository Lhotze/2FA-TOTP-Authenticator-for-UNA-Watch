#ifndef MODEL_HPP
#define MODEL_HPP

#include "touchgfx/UIEventListener.hpp"

#include "SDK/Kernel/Kernel.hpp"
#include "SDK/Interfaces/IGuiLifeCycleCallback.hpp"
#include "SDK/Interfaces/ICustomMessageHandler.hpp"

#include <ta/Platform.hpp>

#include "gui/common/GuiConfig.hpp"

#include <memory>

class FrontendApplication;
class ModelListener;
class WatchPlatform;

class Model : public touchgfx::UIEventListener,
              public SDK::Interface::IGuiLifeCycleCallback,
              public SDK::Interface::ICustomMessageHandler
{
public:
    Model();
    ~Model();

    void bind(ModelListener *listener)
    {
        modelListener = listener;
    }

    FrontendApplication &application();
    void tick();

    /**
     * @brief Exits the application.
     * This method notifies the kernel that the application is exiting and performs
     * any necessary cleanup before termination.
     */
    void exitApp();

    /// The files of the app folder and the clock: what the program needs from the watch.
    ta::Platform &platform();

    uint32_t nowMs() const;
    uint32_t utc() const;                       ///< seconds since 1970 (UTC) from the clock of the watch, 0: not known
    void setBacklight(bool on, int percent);   ///< the screen stays lit or goes back to the usual timeout

    // ICustomMessageHandler
    bool customMessageHandler(SDK::MessageBase *msg) override;

protected:
    ModelListener* modelListener;           ///< Pointer to model listener

    const SDK::Kernel& mKernel;             ///< Reference to kernel interface

    bool mInvalidate = false;               ///< Request to redraw current screen
    uint32_t mUtc = 0, mUtcAtMs = 0;        ///< the time, as the Service told it last, and when

    std::unique_ptr<WatchPlatform> mPlatform;

    // IUserApp implementation
    virtual void onStart()   override;
    virtual void onResume()  override;
    virtual void onStop()    override;
    virtual void onSuspend() override;
};

#endif // MODEL_HPP
