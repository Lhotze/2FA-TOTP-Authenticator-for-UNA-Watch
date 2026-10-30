#ifndef MAINVIEW_HPP
#define MAINVIEW_HPP

#include <gui_generated/main_screen/MainViewBase.hpp>
#include <gui/main_screen/MainPresenter.hpp>
#include <touchgfx/widgets/PixelDataWidget.hpp>

#include <ta/App.hpp>
#include <ta/Draw.hpp>

#include <cstddef>
#include <cstdint>
#include <new>

// The whole screen is one picture that ta::App draws (the code, the lists). It needs no more than the 10 ticks a second the system gives.
class MainView : public MainViewBase
{
public:
    MainView();
    virtual ~MainView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent() override;

    void sleepNow();            // the screen goes away

protected:
    virtual void handleKeyEvent(uint8_t key) override;

private:
#if defined(SIMULATOR)
    void simScript();
#endif

    ta::App                  *mApp;             // lives in static memory (too much for the heap of the process)
    touchgfx::PixelDataWidget mScreen;
    bool                      mExiting;
    uint8_t                   mBuf[ta::draw::kW * ta::draw::kH];
};

#endif // MAINVIEW_HPP
