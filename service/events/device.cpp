#include "device.h"

#include "eventloop.h"
#include "xserverproxy.h"

static unsigned int ignore_mods[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
static unsigned char device_mask_data[2];
static XIEventMask device_mask;
static unsigned char raw_mask_data[3];
static XIEventMask raw_mask;

Events::XiDevice::XiDevice(XIDeviceInfo *info)
        : absolute(false), active(true), proximity_axis(-1), scale_x(1.0), scale_y(1.0), num_buttons(0) {
    dev = info->deviceid;
    name = info->name;
    master = info->attachment;
    for (int j = 0; j < info->num_classes; j++) {
        XIAnyClassInfo *dev_class = info->classes[j];
        if (dev_class->type == ButtonClass) {
            auto *b = (XIButtonClassInfo*)dev_class;
            num_buttons = b->num_buttons;
        } else if (dev_class->type == ValuatorClass) {
            auto *v = (XIValuatorClassInfo *) dev_class;
            if ((v->number == 0 || v->number == 1) && v->mode != XIModeRelative) {
                absolute = true;
                if (v->number == 0) {
                    scale_x = (double) global_xServer->getDisplayWidth() /
                              (double) (v->max - v->min);
                } else {
                    scale_y = (double) global_xServer->getDisplayHeight() /
                              (double) (v->max - v->min);
                }
            }

            if (v->label == global_xServer->atoms.PROXIMITY) {
                proximity_axis = v->number;
            }
        }
    }

    g_message("Opened Device %d ('%s'%s).", dev, info->name, absolute ? ": absolute" : "");
}

void Events::XiDevice::grab_button(ButtonInfo &bi, bool grab) const {
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
        for (int i = 0; i < 4; i++)
            modifiers[i].modifiers = bi.state ^ ignore_mods[i];
    }
    if (grab)
        global_xServer->grabInterfaceButton(dev, bi.button, global_xServer->ROOT, None, GrabModeAsync, GrabModeAsync,
                                            False, &device_mask, nmods, modifiers);
    else {
        global_xServer->ungrabInterfaceButton(dev, bi.button, global_xServer->ROOT, nmods, modifiers);
        global_eventLoop->ungrab(dev);
    }
}


void Events::XiDevice::grab_device(Events::GrabState grab) {
    if (grab == Events::GrabNo) {
        global_xServer->ungrabDevice(dev, CurrentTime);
        global_eventLoop->ungrab(dev);
        return;
    }
    global_xServer->grabDevice(dev, global_xServer->ROOT, CurrentTime, None, GrabModeAsync, GrabModeAsync, False,
                               grab == Events::GrabYes ? &device_mask : &raw_mask);
}

void Events::XiDevice::init() {
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
