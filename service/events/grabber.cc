#include "grabber.h"

#include "handler.h"
#include "eventloop.h"

Grabber::Grabber() : Grabber(std::make_shared<Events::DeviceObserver>()) {
}

Grabber::Grabber(const std::shared_ptr<Events::DeviceObserver>& deviceObserver)
        : pointer(std::make_unique<Events::PointerGrabber>()),
          devices(deviceObserver),
          deviceGrabber(std::make_unique<Events::DeviceGrabber>(deviceObserver)),
          buttonGrabber(std::make_unique<Events::ButtonGrabber>(deviceObserver)) {
    current = BUTTON;
    suspended = 0;
    suspend();
    active = true;
    grabbed = NONE;

    set();

    update();
    resume();
}


void Grabber::set() {
    bool act = !suspended && (active || (current != NONE && current != BUTTON));
    this->buttonGrabber->grab_xi(act && current != SELECT);
    if (!act)
        this->deviceGrabber->grab_xi_devs(Events::GrabNo);
    else if (current == NONE)
        this->deviceGrabber->grab_xi_devs(Events::GrabYes);
    else if (current == RAW)
        this->deviceGrabber->grab_xi_devs(Events::GrabRaw);
    else
        this->deviceGrabber->grab_xi_devs(Events::GrabNo);
    auto old = grabbed;
    grabbed = act ? current : NONE;
    if (old == grabbed)
        return;

    if (old == SELECT) {
        this->pointer->ungrab();
    }

    if (grabbed == SELECT) {
        this->pointer->grab();
    }
}


void Grabber::update() {
    auto bi = prefs.button;
    active = true;
    auto i = prefs.exceptions->find(global_eventLoop->windowObserver->getCurrentWindowClass());
    if (i != prefs.exceptions->end()) {
        if (i->second) {
            bi = *i->second;
        } else {
            active = false;
        }
    }

    if (actions.disAllowApplication(global_eventLoop->windowObserver->getCurrentWindowClass())) {
        active = false;
    }

    const auto extra = prefs.extra_buttons;
    if (this->buttonGrabber->isFirstButton(bi)) {
        set();
        return;
    }
    suspend();
    this->buttonGrabber->setGrabbedButton(bi);
    resume();
}
