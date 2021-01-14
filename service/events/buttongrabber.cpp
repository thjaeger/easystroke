#include "buttongrabber.h"

namespace Events {

    ButtonGrabber::ButtonGrabber(std::shared_ptr<Events::DeviceObserver> devices)
            : devices(std::move(devices)),
              xi_grabbed(false) {
        grabbed_button.button = 0;
        grabbed_button.state = 0;
    }

    void ButtonGrabber::grab_xi(bool grab) {
        if (!xi_grabbed == !grab) {
            return;
        }
        xi_grabbed = grab;
        for (auto &xi_dev : this->devices->xi_devs) {
            if (xi_dev.second->active) {
                for (auto &button : buttons) {
                    xi_dev.second->grab_button(button, grab);
                }
            }
        }
    }

    bool ButtonGrabber::is_instant(guint b) {
        for (auto &button : buttons)
            if (button.button == b && button.instant)
                return true;
        return false;
    }

    bool ButtonGrabber::is_click_hold(guint b) {
        for (auto &button : buttons)
            if (button.button == b && button.click_hold)
                return true;
        return false;
    }

    guint ButtonGrabber::get_default_mods(guint button) {
        for (auto &i : buttons)
            if (i.button == button)
                return i.state;
        return AnyModifier;
    }

    bool ButtonGrabber::isFirstButton(ButtonInfo bi) {
        const auto extra = prefs.extra_buttons;

        return grabbed_button == bi && buttons.size() == extra->size() + 1 &&
               std::equal(extra->begin(), extra->end(), ++buttons.begin());
    }

    void ButtonGrabber::setGrabbedButton(ButtonInfo bi) {
        const auto extra = prefs.extra_buttons;
        grabbed_button = bi;
        buttons.clear();
        buttons.reserve(extra->size() + 1);
        buttons.push_back(bi);
        for (auto &i : *extra)
            if (!i.overlap(bi))
                buttons.push_back(i);
    }
}
