#include "deviceobserver.h"

#include <gtkmm.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>

#include "eventloop.h"
#include "xserverproxy.h"

namespace Events {

    DeviceObserver::DeviceObserver() {
        /* XInput Extension available? */
        int major = 2, minor = 0;
        if (!global_xServer->queryExtension("XInputExtension", &opcode, &event, &error) ||
            global_xServer->queryInterfaceVersion(&major, &minor) == BadRequest ||
            major < 2) {
            g_error("This version of Easy Gesture needs an XInput 2.0-aware X server.\n"
                    "Please upgrade your X server to 1.7.");
        }

        int n;
        auto *info = global_xServer->queryDevice(XIAllDevices, &n);
        if (!info) {
            g_warning("Warning: No XInput devices available");
            return;
        }

        for (int i = 0; i < n; i++) {
            new_device(info + i);
        }

        XIFreeDeviceInfo(info);
        update_excluded();

        if (xi_devs.empty()) {
            g_error("Error: No suitable XInput devices found");
        }

        Events::XiDevice::init();

        XIEventMask global_mask;
        unsigned char data[2] = {0, 0};
        global_mask.deviceid = XIAllDevices;
        global_mask.mask = data;
        global_mask.mask_len = sizeof(data);
        XISetMask(global_mask.mask, XI_HierarchyChanged);

        global_xServer->selectInterfaceEvents(global_xServer->ROOT, &global_mask, 1);
    }

    bool is_xtest_device(int dev) {
        Atom type;
        int format;
        unsigned long num_items, bytes_after;
        unsigned char *data;
        if (Success != global_xServer->getInterfaceProperty(dev, global_xServer->atoms.XTEST, 0, 1, False, XA_INTEGER,
                                                            &type, &format, &num_items, &bytes_after, &data))
            return false;
        bool ret = num_items && format == 8 && *((int8_t *) data);
        XServerProxy::free(data);
        return ret;
    }

    void DeviceObserver::new_device(XIDeviceInfo *info) {
        if (info->use == XIMasterPointer || info->use == XIMasterKeyboard) { return; }

        if (is_xtest_device(info->deviceid)) { return; }

        for (int j = 0; j < info->num_classes; j++) {
            if (info->classes[j]->type == ButtonClass) {
                auto *xi_dev = new Events::XiDevice(info);
                xi_devs[info->deviceid].reset(xi_dev);
                return;
            }
        }
    }

    XiDevice *DeviceObserver::get_xi_dev(int id) {
        auto i = xi_devs.find(id);
        return i == xi_devs.end() ? nullptr : i->second.get();
    }

    void DeviceObserver::update_excluded() {
        for (auto &xi_dev : xi_devs)
            xi_dev.second->active = !prefs.excluded_devices->count(xi_dev.second->name);
    }

    bool DeviceObserver::hierarchy_changed(XIHierarchyEvent *event) {
        bool changed = false;
        for (int i = 0; i < event->num_info; i++) {
            XIHierarchyInfo *info = event->info + i;
            if (info->flags & XISlaveAdded) {
                int n;
                auto *dev_info = global_xServer->queryDevice(info->deviceid, &n);
                if (!dev_info)
                    continue;
                new_device(dev_info);
                XIFreeDeviceInfo(dev_info);
                update_excluded();
                changed = true;
            } else if (info->flags & XISlaveRemoved) {
                g_message("Device %d removed.", info->deviceid);
                global_eventLoop->remove_device(info->deviceid);
                xi_devs.erase(info->deviceid);
                changed = true;
            } else if (info->flags & (XISlaveAttached | XISlaveDetached)) {
                auto i = xi_devs.find(info->deviceid);
                if (i != xi_devs.end())
                    i->second->master = info->attachment;
            }
        }
        return changed;
    }
}
