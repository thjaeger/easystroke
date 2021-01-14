#include "devicegrabber.h"

namespace Events {
    DeviceGrabber::DeviceGrabber(std::shared_ptr<Events::DeviceObserver> devices)
            : devices(std::move(devices)), xi_devs_grabbed(Events::GrabNo) {

    }

    void DeviceGrabber::grab_xi_devs(Events::GrabState grab) {
        if (xi_devs_grabbed == grab) {
            return;
        }

        xi_devs_grabbed = grab;
        for (auto &xi_dev : this->devices->xi_devs) {
            xi_dev.second->grab_device(grab);
        }
    }
}
