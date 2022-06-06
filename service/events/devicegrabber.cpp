#include "devicegrabber.h"

#include "eventloop.h"
#include "xserverproxy.h"


namespace Events {
    static unsigned char raw_mask_data[3];
    static XIEventMask raw_mask;

    static unsigned char device_mask_data[2];
    static XIEventMask device_mask;


    DeviceGrabber::DeviceGrabber(std::shared_ptr<Events::DeviceObserver> devices)
            : devices(std::move(devices)), xi_devs_grabbed(Events::GrabNo) {
        device_mask.deviceid = XIAllDevices;
        device_mask.mask = device_mask_data;
        device_mask.mask_len = sizeof(device_mask_data);
        memset(device_mask.mask, 0, device_mask.mask_len);
        XISetMask(device_mask.mask, XI_ButtonPress);
        XISetMask(device_mask.mask, XI_ButtonRelease);
        XISetMask(device_mask.mask, XI_Motion);

        raw_mask.deviceid = XIAllDevices;
        raw_mask.mask = raw_mask_data;
        raw_mask.mask_len = sizeof(raw_mask_data);
        memset(raw_mask.mask, 0, raw_mask.mask_len);
        XISetMask(raw_mask.mask, XI_ButtonPress);
        XISetMask(raw_mask.mask, XI_ButtonRelease);
        XISetMask(raw_mask.mask, XI_RawMotion);
    }

    void DeviceGrabber::grab_xi_devs(Events::GrabState grab) {
        if (xi_devs_grabbed == grab) {
            return;
        }

        xi_devs_grabbed = grab;
        for (auto &xi_dev : this->devices->xi_devs) {
            this->grab_device(*xi_dev.second, grab);
        }
    }

    void DeviceGrabber::grab_device(XiDevice& device, GrabState grab) {
        if (grab == Events::GrabNo) {
            global_xServer->ungrabDevice(device.dev, CurrentTime);
            global_eventLoop->ungrab(device.dev);
            return;
        }
        global_xServer->grabDevice(device.dev, global_xServer->ROOT, CurrentTime, None, GrabModeAsync, GrabModeAsync, False,
                                   grab == Events::GrabYes ? &device_mask : &raw_mask);
    }
}
