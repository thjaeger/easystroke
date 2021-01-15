#include "buttongrabber.h"

#include "eventloop.h"
#include "xserverproxy.h"

namespace Events {
    static unsigned int ignore_mods[4] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    static unsigned char device_mask_data[2];
    static XIEventMask device_mask;

    ButtonGrabber::ButtonGrabber(std::shared_ptr<Events::DeviceObserver> devices)
            : devices(std::move(devices)),
              xi_grabbed(false) {
        grabbed_button.button = 0;
        grabbed_button.state = 0;

        device_mask.deviceid = XIAllDevices;
        device_mask.mask = device_mask_data;
        device_mask.mask_len = sizeof(device_mask_data);
        memset(device_mask.mask, 0, device_mask.mask_len);
        XISetMask(device_mask.mask, XI_ButtonPress);
        XISetMask(device_mask.mask, XI_ButtonRelease);
        XISetMask(device_mask.mask, XI_Motion);
    }

    void ButtonGrabber::grab_xi(bool grab) {
        if (!xi_grabbed == !grab) {
            return;
        }
        xi_grabbed = grab;
        for (auto &xi_dev : this->devices->xi_devs) {
            if (xi_dev.second->active) {
                for (auto &button : buttons) {
                    this->grab_button(*xi_dev.second, button, grab);
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

    void ButtonGrabber::grab_button(XiDevice &device, ButtonInfo &bi, bool grab) const {
        XIGrabModifiers modifiers[4] = {{0, 0},
                                        {0, 0},
                                        {0, 0},
                                        {0, 0}};
        int nmods;
        if (bi.button == AnyModifier) {
            nmods = 1;
            modifiers[0].modifiers = XIAnyModifier;
        } else {
            nmods = 4;
            for (int i = 0; i < 4; i++) { modifiers[i].modifiers = bi.state ^ ignore_mods[i]; }
        }
        if (grab) {
            global_xServer->grabInterfaceButton(device.dev, bi.button, global_xServer->ROOT, None, GrabModeAsync,
                                                GrabModeAsync,
                                                False, &device_mask, nmods, modifiers);
        } else {
            global_xServer->ungrabInterfaceButton(device.dev, bi.button, global_xServer->ROOT, nmods, modifiers);
            global_eventLoop->ungrab(device.dev);
        }
    }
}
