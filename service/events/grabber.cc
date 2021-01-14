#include "grabber.h"

#include "handler.h"
#include "eventloop.h"
#include <X11/Xutil.h>


Grabber::Grabber() : pointer(std::make_unique<Events::PointerGrabber>()),
                     devices(std::make_unique<Events::DeviceObserver>()) {
    current = BUTTON;
    suspended = 0;
    suspend();
    active = true;
    grabbed = NONE;
    xi_grabbed = false;
    xi_devs_grabbed = Events::GrabNo;
    grabbed_button.button = 0;
    grabbed_button.state = 0;

    xi_grabbed = false;
    set();

    update();
    resume();
}

void Grabber::grab_xi(bool grab) {
    if (!xi_grabbed == !grab)
        return;
    xi_grabbed = grab;
    for (auto &xi_dev : this->devices->xi_devs)
        if (xi_dev.second->active)
            for (auto &button : buttons)
                xi_dev.second->grab_button(button, grab);
}

void Grabber::grab_xi_devs(Events::GrabState grab) {
    if (xi_devs_grabbed == grab)
        return;
    xi_devs_grabbed = grab;
    for (auto &xi_dev : this->devices->xi_devs)
        xi_dev.second->grab_device(grab);
}

void Grabber::set() {
    bool act = !suspended && (active || (current != NONE && current != BUTTON));
    grab_xi(act && current != SELECT);
    if (!act)
        grab_xi_devs(Events::GrabNo);
    else if (current == NONE)
        grab_xi_devs(Events::GrabYes);
    else if (current == RAW)
        grab_xi_devs(Events::GrabRaw);
    else
        grab_xi_devs(Events::GrabNo);
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

bool Grabber::is_instant(guint b) {
    for (auto &button : buttons)
        if (button.button == b && button.instant)
            return true;
    return false;
}

bool Grabber::is_click_hold(guint b) {
    for (auto &button : buttons)
        if (button.button == b && button.click_hold)
            return true;
    return false;
}

guint Grabber::get_default_mods(guint button) {
    for (auto &i : buttons)
        if (i.button == button)
            return i.state;
    return AnyModifier;
}

void Grabber::update() {
    auto bi = prefs.button;
    active = true;
    auto i = prefs.exceptions->find(global_eventLoop->windowObserver->getCurrentWindowClass());
    if (i != prefs.exceptions->end()) {
        if (i->second)
            bi = *i->second;
        else
            active = false;
    }

    if (actions.disAllowApplication(global_eventLoop->windowObserver->getCurrentWindowClass())) {
        active = false;
    }

    const auto extra = prefs.extra_buttons;
    if (grabbed_button == bi && buttons.size() == extra->size() + 1 &&
        std::equal(extra->begin(), extra->end(), ++buttons.begin())) {
        set();
        return;
    }
    suspend();
    grabbed_button = bi;
    buttons.clear();
    buttons.reserve(extra->size() + 1);
    buttons.push_back(bi);
    for (auto &i : *extra)
        if (!i.overlap(bi))
            buttons.push_back(i);
    resume();
}
