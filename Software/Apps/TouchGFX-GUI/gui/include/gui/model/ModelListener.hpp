#ifndef MODELLISTENER_HPP
#define MODELLISTENER_HPP

#include <gui/model/Model.hpp>
#include <gui/common/FrontendApplication.hpp>

class ModelListener
{
public:
    ModelListener() : model(0) {}
    
    virtual ~ModelListener() {}

    void bind(Model* m)
    {
        model = m;
    }

    virtual void onIdleTimeout() {}
    virtual void onSleep() {}      // the screen goes away (suspended or stopped): save what must not be lost

protected:
    Model* model;

    
};

#endif // MODELLISTENER_HPP
