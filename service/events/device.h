#pragma once

#include <gtkmm.h>
#include <X11/extensions/XInput2.h>

#include "prefdb.h"

namespace Events {
    enum GrabState { GrabNo, GrabYes, GrabRaw };

    struct XiDevice {
        int dev;
        std::string name;
        bool absolute;
        bool active;
        int proximity_axis;
        double scale_x, scale_y;
        int num_buttons;
        int master;
        explicit XiDevice(XIDeviceInfo *info);
        void grab_device(Events::GrabState grab);
        void grab_button(ButtonInfo &bi, bool grab) const;

        static void init();
    };
}
