#pragma once

#include <memory>

#include "deviceobserver.h"

namespace Events {
    class DeviceGrabber {
    private:
        std::shared_ptr<Events::DeviceObserver> devices;

    public:
        explicit DeviceGrabber(std::shared_ptr<Events::DeviceObserver> devices);

        Events::GrabState xi_devs_grabbed;

        void grab_xi_devs(Events::GrabState);
    };
}
