/*
 * Copyright (c) 2008-2009, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "actiondb.h" // TODO
#include "grabber.h"
#include "main.h"
#include "prefs.h"
#include <X11/extensions/XTest.h>
#include <xorg/xserver-properties.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>
#include <glibmm/i18n.h>

extern Window get_window(Window w, Atom prop);
extern Source<bool> disabled;
extern Source<Window> current_app_window;
extern Source<bool> recording;

Grabber *grabber = 0;

static unsigned int ignore_mods[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
static unsigned char device_mask_data[2];
static XIEventMask device_mask;
static unsigned char raw_mask_data[3];
static XIEventMask raw_mask;

template <class X1, class X2> class BiMap {
	std::map<X1, X2> map1;
	std::map<X2, X1> map2;
public:
	void erase1(X1 x1) {
		typename std::map<X1, X2>::iterator i1 = map1.find(x1);
		if (i1 == map1.end())
			return;
		map2.erase(i1->second);
		map1.erase(i1->first);
	}
	void erase2(X2 x2) {
		typename std::map<X2, X1>::iterator i2 = map2.find(x2);
		if (i2 == map2.end())
			return;
		map1.erase(i2->second);
		map2.erase(i2->first);
	}
	void pop(X1 &x1, X2 &x2) {
		typename std::map<X1, X2>::reverse_iterator i1 = map1.rbegin();
		x1 = i1->first;
		x2 = i1->second;
		map2.erase(i1->second);
		map1.erase(i1->first);
	}
	void add(X1 x1, X2 x2) {
		erase1(x1);
		erase2(x2);
		map1[x1] = x2;
		map2[x2] = x1;
	}
	bool empty() { return map1.empty(); }
	bool contains1(X1 x1) { return map1.find(x1) != map1.end(); }
	bool contains2(X2 x2) { return map2.find(x2) != map2.end(); }
	X2 find1(X1 x1) { return map1.find(x1)->second; }
	X1 find2(X2 x2) { return map2.find(x2)->second; }
};

Atom XAtom::operator*() {
	if (!atom)
		atom = XInternAtom(dpy, name, False);
	return atom;
}

BiMap<Window, Window> frame_win;
BiMap<Window, Window> frame_child;
XAtom _NET_FRAME_WINDOW("_NET_FRAME_WINDOW");
XAtom _NET_WM_STATE("_NET_WM_STATE");
XAtom _NET_WM_STATE_HIDDEN("_NET_WM_STATE_HIDDEN");
XAtom _NET_ACTIVE_WINDOW("_NET_ACTIVE_WINDOW");

BiMap<unsigned int, Window> minimized;
unsigned int minimized_n = 0;

void get_frame(Window w) {
	Window frame = get_window(w, *_NET_FRAME_WINDOW);
	if (!frame)
		return;
	frame_win.add(frame, w);
}

Children::Children(Window w) : parent(w) {
	XSelectInput(dpy, parent, SubstructureNotifyMask);
	unsigned int n;
	Window dummyw1, dummyw2, *ch;
	XQueryTree(dpy, parent, &dummyw1, &dummyw2, &ch, &n);
	for (unsigned int i = 0; i < n; i++)
		add(ch[i]);
	XFree(ch);
}

bool Children::handle(XEvent &ev) {
	switch (ev.type) {
		case CreateNotify:
			if (ev.xcreatewindow.parent != parent)
				return false;
			add(ev.xcreatewindow.window);
			return true;
		case DestroyNotify:
			frame_child.erase1(ev.xdestroywindow.window);
			frame_child.erase2(ev.xdestroywindow.window);
			minimized.erase2(ev.xdestroywindow.window);
			destroy(ev.xdestroywindow.window);
			return true;
		case ReparentNotify:
			if (ev.xreparent.event != parent)
				return false;
			if (ev.xreparent.window == parent)
				return false;
			if (ev.xreparent.parent == parent)
				add(ev.xreparent.window);
			else
				remove(ev.xreparent.window);
			return true;
		case PropertyNotify:
			if (ev.xproperty.atom == *_NET_FRAME_WINDOW) {
				if (ev.xproperty.state == PropertyDelete)
					frame_win.erase1(ev.xproperty.window);
				if (ev.xproperty.state == PropertyNewValue)
					get_frame(ev.xproperty.window);
				return true;
			}
			if (ev.xproperty.atom == *_NET_WM_STATE) {
				if (ev.xproperty.state == PropertyDelete) {
					minimized.erase2(ev.xproperty.window);
					return true;
				}
				if (has_atom(ev.xproperty.window, *_NET_WM_STATE, *_NET_WM_STATE_HIDDEN))
					minimized.add(minimized_n++, ev.xproperty.window);
				else
					minimized.erase2(ev.xproperty.window);
				return true;
			}
			return false;
		default:
			return false;
	}
}

void Children::add(Window w) {
	if (!w)
		return;

	XSelectInput(dpy, w, EnterWindowMask | PropertyChangeMask);
	get_frame(w);
}

void Children::remove(Window w) {
	XSelectInput(dpy, w, 0);
	destroy(w);
}

void Children::destroy(Window w) {
	frame_win.erase1(w);
	frame_win.erase2(w);
}

static void activate(Window w, Time t) {
	XClientMessageEvent ev;
	ev.type = ClientMessage;
	ev.window = w;
	ev.message_type = *_NET_ACTIVE_WINDOW;
	ev.format = 32;
	ev.data.l[0] = 0; // 1 app, 2 pager
	ev.data.l[1] = t;
	ev.data.l[2] = 0;
	ev.data.l[3] = 0;
	ev.data.l[4] = 0;
	XSendEvent(dpy, ROOT, False, SubstructureNotifyMask | SubstructureRedirectMask, (XEvent *)&ev);
}

std::string get_wm_class(Window w) {
	if (!w)
		return "";
	XClassHint ch;
	if (!XGetClassHint(dpy, w, &ch))
		return "";
	std::string ans = ch.res_name;
	XFree(ch.res_name);
	XFree(ch.res_class);
	return ans;
}

class IdleNotifier : public Base {
	sigc::slot<void> f;
	void run() { f(); }
public:
	IdleNotifier(sigc::slot<void> f_) : f(f_) {}
	virtual void notify() { queue(sigc::mem_fun(*this, &IdleNotifier::run)); }
};

void Grabber::unminimize() {
	if (minimized.empty())
		return;
	Window w;
	unsigned int n;
	minimized.pop(n, w);
	activate(w, CurrentTime);
}

const char *Grabber::state_name[4] = { "None", "Button", "Select", "Raw" };

Grabber::Grabber() : children(ROOT) {
	current = BUTTON;
	suspended = 0;
	suspend();
	active = true;
	grabbed = NONE;
	xi_grabbed = false;
	xi_devs_grabbed = GrabNo;
	grabbed_button.button = 0;
	grabbed_button.state = 0;
	cursor_select = XCreateFontCursor(dpy, XC_crosshair);
	init_xi();
	prefs.excluded_devices.connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::update)));
	prefs.button.connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::update)));
	current_class = fun(&get_wm_class, current_app_window);
	current_class->connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::update)));
	recording.connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::update)));
	disabled.connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::set)));
	update();
	resume();
}

Grabber::~Grabber() {
	XFreeCursor(dpy, cursor_select);
}

bool Grabber::init_xi() {
	/* XInput Extension available? */
	int major = 2, minor = 0;
	if (!XQueryExtension(dpy, "XInputExtension", &opcode, &event, &error) ||
			XIQueryVersion(dpy, &major, &minor) == BadRequest ||
			major < 2) {
		printf("Error: This version of easystroke needs an XInput 2.0-aware X server.\n"
				"Please downgrade to easystroke 0.4.x or upgrade your X server to 1.7.\n");
		exit(EXIT_FAILURE);
	}

	int n;
	XIDeviceInfo *info = XIQueryDevice(dpy, XIAllDevices, &n);
	if (!info) {
		printf("Warning: No XInput devices available\n");
		return false;
	}

	current_dev = NULL;

	for (int i = 0; i < n; i++)
		new_device(info + i);
	XIFreeDeviceInfo(info);
	prefs.excluded_devices.connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::update_excluded)));
	update_excluded();
	xi_grabbed = false;
	set();

	if (!xi_devs.size()) {
		printf("Error: No suitable XInput devices found\n");
		exit(EXIT_FAILURE);
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
	unsigned char data[2] = { 0, 0 };
	global_mask.deviceid = XIAllDevices;
	global_mask.mask = data;
	global_mask.mask_len = sizeof(data);
	XISetMask(global_mask.mask, XI_HierarchyChanged);

	XISelectEvents(dpy, ROOT, &global_mask, 1);

	return true;
}

bool Grabber::hierarchy_changed(XIHierarchyEvent *event) {
	bool changed = false;
	for (int i = 0; i < event->num_info; i++) {
		XIHierarchyInfo *info = event->info + i;
		if (info->flags & XISlaveAdded) {
			int n;
			XIDeviceInfo *dev_info = XIQueryDevice(dpy, info->deviceid, &n);
			if (!dev_info)
				continue;
			new_device(dev_info);
			XIFreeDeviceInfo(dev_info);
			update_excluded();
			changed = true;
		} else if (info->flags & XISlaveRemoved) {
			if (verbosity >= 1)
				printf("Device %d removed.\n", info->deviceid);
			xi_devs.erase(info->deviceid);
			if (current_dev && current_dev->dev == info->deviceid)
				current_dev = NULL;
			changed = true;
		} else if (info->flags & (XISlaveAttached | XISlaveDetached)) {
			DeviceMap::iterator i = xi_devs.find(info->deviceid);
			if (i != xi_devs.end())
				i->second->master = info->attachment;
		}
	}
	return changed;
}

void Grabber::update_excluded() {
	suspend();
	for (DeviceMap::iterator i = xi_devs.begin(); i != xi_devs.end(); ++i)
		i->second->active = !prefs.excluded_devices.ref().count(i->second->name);
	resume();
}

bool is_xtest_device(int dev) {
	static XAtom XTEST(XI_PROP_XTEST_DEVICE);
	Atom type;
	int format;
	unsigned long num_items, bytes_after;
	unsigned char *data;
	if (Success != XIGetProperty(dpy, dev, *XTEST, 0, 1, False, XA_INTEGER,
				&type, &format, &num_items, &bytes_after, &data))
		return false;
	bool ret = num_items && format == 8 && *((int8_t*)data);
	XFree(data);
	return ret;
}

void Grabber::new_device(XIDeviceInfo *info) {
	if (info->use == XIMasterPointer || info->use == XIMasterKeyboard)
		return;

	if (is_xtest_device(info->deviceid))
		return;

	for (int j = 0; j < info->num_classes; j++)
		if (info->classes[j]->type == ButtonClass) {
			XiDevice *xi_dev = new XiDevice(this, info);
			xi_devs[info->deviceid].reset(xi_dev);
			return;
		}
}

Grabber::XiDevice::XiDevice(Grabber *parent, XIDeviceInfo *info) : absolute(false), proximity_axis(-1), scale_x(1.0), scale_y(1.0), num_buttons(0) {
	static XAtom PROXIMITY(AXIS_LABEL_PROP_ABS_DISTANCE);
	dev = info->deviceid;
	name = info->name;
	master = info->attachment;
	for (int j = 0; j < info->num_classes; j++) {
		XIAnyClassInfo *dev_class = info->classes[j];
		if (dev_class->type == ButtonClass) {
			XIButtonClassInfo *b = (XIButtonClassInfo*)dev_class;
			num_buttons = b->num_buttons;
		} else if (dev_class->type == ValuatorClass) {
			XIValuatorClassInfo *v = (XIValuatorClassInfo*)dev_class;
			if ((v->number == 0 || v->number == 1) && v->mode != XIModeRelative) {
				absolute = true;
				if (v-> number == 0)
					scale_x = (double)DisplayWidth(dpy, DefaultScreen(dpy)) / (double)(v->max - v->min);
				else
					scale_y = (double)DisplayHeight(dpy, DefaultScreen(dpy)) / (double)(v->max - v->min);

			}
			if (v->label == *PROXIMITY)
				proximity_axis = v->number;
		}
	}

	if (verbosity >= 1)
		printf("Opened Device %d ('%s'%s).\n", dev, info->name, absolute ? ": absolute" : "");
}

Grabber::XiDevice *Grabber::get_xi_dev(int id) {
	DeviceMap::iterator i = xi_devs.find(id);
	return i == xi_devs.end() ? NULL : i->second.get();
}

void Grabber::XiDevice::grab_button(ButtonInfo &bi, bool grab) {
	XIGrabModifiers modifiers[4] = {{0,0},{0,0},{0,0},{0,0}};
	int nmods = 0;
	if (bi.button == AnyModifier) {
		nmods = 1;
		modifiers[0].modifiers = XIAnyModifier;
	} else {
		nmods = 4;
		for (int i = 0; i < 4; i++)
			modifiers[i].modifiers = bi.state ^ ignore_mods[i];
	}
	if (grab)
		XIGrabButton(dpy, dev, bi.button, ROOT, None, GrabModeAsync, GrabModeAsync, False, &device_mask, nmods, modifiers);
	else {
		XIUngrabButton(dpy, dev, bi.button, ROOT, nmods, modifiers);
		if (current_dev && current_dev->dev == dev)
			xinput_pressed.clear();
	}
}

void Grabber::grab_xi(bool grab) {
	if (!xi_grabbed == !grab)
		return;
	xi_grabbed = grab;
	for (DeviceMap::iterator i = xi_devs.begin(); i != xi_devs.end(); ++i)
		if (i->second->active)
			for (std::vector<ButtonInfo>::iterator j = buttons.begin(); j != buttons.end(); j++)
				i->second->grab_button(*j, grab);
}

void Grabber::XiDevice::grab_device(GrabState grab) {
	if (grab == GrabNo) {
		XIUngrabDevice(dpy, dev, CurrentTime);
		if (current_dev && current_dev->dev == dev)
			xinput_pressed.clear();
		return;
	}
	XIGrabDevice(dpy, dev, ROOT, CurrentTime, None, GrabModeAsync, GrabModeAsync, False,
			grab == GrabYes ? &device_mask : &raw_mask);
}

void Grabber::grab_xi_devs(GrabState grab) {
	if (xi_devs_grabbed == grab)
		return;
	xi_devs_grabbed = grab;
	for (DeviceMap::iterator i = xi_devs.begin(); i != xi_devs.end(); ++i)
		i->second->grab_device(grab);
}

void Grabber::set() {
	bool act = !suspended && ((active && !disabled.get()) || (current != NONE && current != BUTTON));
	grab_xi(act && current != SELECT);
	if (!act)
		grab_xi_devs(GrabNo);
	else if (current == NONE)
		grab_xi_devs(GrabYes);
	else if (current == RAW)
		grab_xi_devs(GrabRaw);
	else
		grab_xi_devs(GrabNo);
	State old = grabbed;
	grabbed = act ? current : NONE;
	if (old == grabbed)
		return;
	if (verbosity >= 2)
		printf("grabbing: %s\n", state_name[grabbed]);

	if (old == SELECT)
		XUngrabPointer(dpy, CurrentTime);

	if (grabbed == SELECT) {
		int code = XGrabPointer(dpy, ROOT, False, ButtonPressMask,
				GrabModeAsync, GrabModeAsync, ROOT, cursor_select, CurrentTime);
		if (code != GrabSuccess)
			throw GrabFailedException(code);
	}
}

bool Grabber::is_grabbed(guint b) {
	for (std::vector<ButtonInfo>::iterator i = buttons.begin(); i != buttons.end(); i++)
		if (i->button == b)
			return true;
	return false;
}

bool Grabber::is_instant(guint b) {
	for (std::vector<ButtonInfo>::iterator i = buttons.begin(); i != buttons.end(); i++)
		if (i->button == b && i->instant)
			return true;
	return false;
}

bool Grabber::is_click_hold(guint b) {
	for (std::vector<ButtonInfo>::iterator i = buttons.begin(); i != buttons.end(); i++)
		if (i->button == b && i->click_hold)
			return true;
	return false;
}

guint Grabber::get_default_mods(guint button) {
	for (std::vector<ButtonInfo>::const_iterator i = buttons.begin(); i != buttons.end(); ++i)
		if (i->button == button)
			return i->state;
	return AnyModifier;
}

void Grabber::update() {
	ButtonInfo bi = prefs.button.ref();
	active = true;
	if (!recording.get()) {
		std::map<std::string, RButtonInfo>::const_iterator i = prefs.exceptions.ref().find(current_class->get());
		if (i != prefs.exceptions.ref().end()) {
			if (i->second)
				bi = *i->second;
			else
				active = false;
		}

		if (prefs.whitelist.get() && !actions.apps.count(current_class->get()))
			active = false;
	}
	const std::vector<ButtonInfo> &extra = prefs.extra_buttons.ref();
	if (grabbed_button == bi && buttons.size() == extra.size() + 1 &&
			std::equal(extra.begin(), extra.end(), ++buttons.begin())) {
		set();
		return;
	}
	suspend();
	grabbed_button = bi;
	buttons.clear();
	buttons.reserve(extra.size() + 1);
	buttons.push_back(bi);
	for (std::vector<ButtonInfo>::const_iterator i = extra.begin(); i != extra.end(); ++i)
		if (!i->overlap(bi))
			buttons.push_back(*i);
	resume();
}

// Fuck Xlib
bool has_wm_state(Window w) {
	static XAtom WM_STATE("WM_STATE");
	Atom actual_type_return;
	int actual_format_return;
	unsigned long nitems_return;
	unsigned long bytes_after_return;
	unsigned char *prop_return;
	if (Success != XGetWindowProperty(dpy, w, *WM_STATE, 0, 2, False,
				AnyPropertyType, &actual_type_return,
				&actual_format_return, &nitems_return,
				&bytes_after_return, &prop_return))
		return false;
	XFree(prop_return);
	return nitems_return;
}

Window find_wm_state(Window w) {
	if (!w)
		return w;
	if (has_wm_state(w))
		return w;
	Window found = None;
	unsigned int n;
	Window dummyw1, dummyw2, *ch;
	if (!XQueryTree(dpy, w, &dummyw1, &dummyw2, &ch, &n))
		return None;
	for (unsigned int i = 0; i != n; i++)
		if (has_wm_state(ch[i]))
			found = ch[i];
	if (!found)
		for (unsigned int i = 0; i != n; i++) {
			found = find_wm_state(ch[i]);
			if (found)
				break;
		}
	XFree(ch);
	return found;
}

Window get_app_window(Window w) {
	if (!w)
		return w;

	if (frame_win.contains1(w))
		return frame_win.find1(w);

	if (frame_child.contains1(w))
		return frame_child.find1(w);

	Window w2 = find_wm_state(w);
	if (w2) {
		frame_child.add(w, w2);
		if (w2 != w) {
			w = w2;
			XSelectInput(dpy, w2, StructureNotifyMask | PropertyChangeMask);
		}
		return w2;
	}
	if (verbosity >= 1)
		printf("Window 0x%lx does not have an associated top-level window\n", w);
	return w;
}
