#pragma once

#include <memory>

#include "deviceobserver.h"

namespace Events {
    class ButtonGrabber {
    private:
        std::shared_ptr<Events::DeviceObserver> devices;
        std::vector<ButtonInfo> buttons;
        ButtonInfo grabbed_button;

    public:
        explicit ButtonGrabber(std::shared_ptr<Events::DeviceObserver> devices);

        bool xi_grabbed;

        void grab_xi(bool);


        bool is_instant(guint b);

        bool is_click_hold(guint b);

        int get_default_button() { return grabbed_button.button; }

        guint get_default_mods(guint button);

        bool isFirstButton(ButtonInfo bi);

        void setGrabbedButton(ButtonInfo bi);

        void grab_button(XiDevice& device, ButtonInfo &bi, bool grab) const;
    };
}
