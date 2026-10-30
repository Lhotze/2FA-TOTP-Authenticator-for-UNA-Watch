#ifndef MAINPRESENTER_HPP
#define MAINPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class MainView;

class MainPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    MainPresenter(MainView& v);

    virtual void activate();
    virtual void deactivate();

    virtual ~MainPresenter() {}

    void exit() { model->exitApp(); }

    ta::Platform &platform() { return model->platform(); }
    uint32_t nowMs() { return model->nowMs(); }

    // from the model
    virtual void onSleep() override;

private:
    MainPresenter();

    MainView& view;
};

#endif // MAINPRESENTER_HPP
