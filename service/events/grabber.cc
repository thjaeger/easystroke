#include "handler.h"
#include "grabber.h"
#include "eventloop.h"
#include "xserverproxy.h"
#include <X11/extensions/XTest.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>

static unsigned int ignore_mods[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
static unsigned char device_mask_data[2];
static XIEventMask device_mask;
static unsigned char raw_mask_data[3];
static XIEventMask raw_mask;

const char *Grabber::state_name[4] = { "None", "Button", "Select", "Raw" };

Grabber::Grabber() {
    current = BUTTON;
    suspended = 0;
    suspend();
    active = true;
    grabbed = NONE;
    xi_grabbed = false;
    xi_devs_grabbed = GrabNo;
    grabbed_button.button = 0;
    grabbed_button.state = 0;
    cursor_select = global_xServer->createFontCursor(XC_crosshair);
    init_xi();

    update();
    resume();
}

Grabber::~Grabber() {
    global_xServer->freeCursor(cursor_select);
}

bool Grabber::init_xi() {
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
        return false;
    }

    for (int i = 0; i < n; i++)
        new_device(info + i);
    XIFreeDeviceInfo(info);
    update_excluded();
    xi_grabbed = false;
    set();

    if (xi_devs.empty()) {
        g_error("Error: No suitable XInput devices found");
    }

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

    XIEventMask global_mask;
    unsigned char data[2] = {0, 0};
    global_mask.deviceid = XIAllDevices;
    global_mask.mask = data;
    global_mask.mask_len = sizeof(data);
    XISetMask(global_mask.mask, XI_HierarchyChanged);

    global_xServer->selectInterfaceEvents(global_xServer->ROOT, &global_mask, 1);

    return true;
}

bool Grabber::hierarchy_changed(XIHierarchyEvent *event) {
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

void Grabber::update_excluded() {
    suspend();
    for (auto &xi_dev : xi_devs)
        xi_dev.second->active = !prefs.excluded_devices->count(xi_dev.second->name);
    resume();
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

void Grabber::new_device(XIDeviceInfo *info) {
    if (info->use == XIMasterPointer || info->use == XIMasterKeyboard) { return; }

    if (is_xtest_device(info->deviceid)) { return; }

    for (int j = 0; j < info->num_classes; j++) {
        if (info->classes[j]->type == ButtonClass) {
            auto *xi_dev = new XiDevice(this, info);
            xi_devs[info->deviceid].reset(xi_dev);
            return;
        }
    }
}

Grabber::XiDevice::XiDevice(Grabber *parent, XIDeviceInfo *info) : absolute(false), active(true), proximity_axis(-1), scale_x(1.0), scale_y(1.0), num_buttons(0) {
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

Grabber::XiDevice *Grabber::get_xi_dev(int id) {
	auto i = xi_devs.find(id);
	return i == xi_devs.end() ? nullptr : i->second.get();
}

void Grabber::XiDevice::grab_button(ButtonInfo &bi, bool grab) const {
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

void Grabber::grab_xi(bool grab) {
    if (!xi_grabbed == !grab)
        return;
    xi_grabbed = grab;
    for (auto &xi_dev : xi_devs)
        if (xi_dev.second->active)
            for (auto &button : buttons)
                xi_dev.second->grab_button(button, grab);
}

void Grabber::XiDevice::grab_device(GrabState grab) {
    if (grab == GrabNo) {
        global_xServer->ungrabDevice(dev, CurrentTime);
        global_eventLoop->ungrab(dev);
        return;
    }
    global_xServer->grabDevice(dev, global_xServer->ROOT, CurrentTime, None, GrabModeAsync, GrabModeAsync, False,
                              grab == GrabYes ? &device_mask : &raw_mask);
}

void Grabber::grab_xi_devs(GrabState grab) {
    if (xi_devs_grabbed == grab)
        return;
    xi_devs_grabbed = grab;
    for (auto &xi_dev : xi_devs)
        xi_dev.second->grab_device(grab);
}

void Grabber::set() {
    bool act = !suspended && (active || (current != NONE && current != BUTTON));
    grab_xi(act && current != SELECT);
    if (!act)
        grab_xi_devs(GrabNo);
    else if (current == NONE)
        grab_xi_devs(GrabYes);
    else if (current == RAW)
        grab_xi_devs(GrabRaw);
    else
        grab_xi_devs(GrabNo);
    auto old = grabbed;
    grabbed = act ? current : NONE;
    if (old == grabbed)
        return;
    g_debug("grabbing: %s", state_name[grabbed]);

    if (old == SELECT)
        global_xServer->ungrabPointer(CurrentTime);

    if (grabbed == SELECT) {
        int code = global_xServer->grabPointer(global_xServer->ROOT, False, ButtonPressMask,
                                              GrabModeAsync, GrabModeAsync, global_xServer->ROOT, cursor_select,
                                              CurrentTime);
        if (code != GrabSuccess) {
            throw GrabFailedException(code);
        }
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
