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
#include "handler.h"
#include "grabber.h"
#include "main.h"
#include "prefs.h"
#include <X11/extensions/XTest.h>
#include <xorg/xserver-properties.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>
#include <glibmm/i18n.h>

extern Source<bool> disabled;
extern Source<Window> current_app_window;
extern Source<bool> recording;

Grabber *grabber = 0;

static unsigned int ignore_mods[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
static unsigned char button_mask_data[4], touch_mask_data[4], raw_mask_data[4];
static XIEventMask button_mask, touch_mask, raw_mask;

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
	Window frame = xstate->get_window(w, *_NET_FRAME_WINDOW);
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
				if (xstate->has_atom(ev.xproperty.window, *_NET_WM_STATE, *_NET_WM_STATE_HIDDEN))
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
	std::string ans = ch.res_class;
	XFree(ch.res_name);
	XFree(ch.res_class);
	return ans;
}

class IdleNotifier : public Base {
	sigc::slot<void> f;
	void run() { f(); }
public:
	IdleNotifier(sigc::slot<void> f_) : f(f_) {}
	virtual void notify() { xstate->queue(sigc::mem_fun(*this, &IdleNotifier::run)); }
};

void Grabber::unminimize() {
	if (minimized.empty())
		return;
	Window w;
	unsigned int n;
	minimized.pop(n, w);
	activate(w, CurrentTime);
}

const char *Grabber::state_name[4] = { "Button", "Grab", "Select", "Raw" };

Grabber::Grabber() :
	children(ROOT),
	current(BUTTON),
	select(false),
	grab_button(false),
	grab_touch(false),
	grab_device(GrabNo),
	touch_grabbed(false),
	suspended(0),
	active(true)
{
	grabber = this;

	grabbed_button.button = 0;
	grabbed_button.state = 0;

	cursor_select = XCreateFontCursor(dpy, XC_crosshair);

	suspend();
	init_xi();
	prefs.excluded_devices.connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::update)));
	prefs.button.connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::update)));
	current_class = fun(&get_wm_class, current_app_window);
	current_class->connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::update)));
	recording.connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::update)));
	disabled.connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::set)));
	prefs.excluded_devices.connect(new IdleNotifier(sigc::mem_fun(*this, &Grabber::update_excluded)));
	update_excluded();
	update();
	resume();
}

Grabber::~Grabber() {
	XFreeCursor(dpy, cursor_select);
}

bool Grabber::init_xi() {
	/* XInput Extension available? */
	int major = 2, minor = 2;
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

	for (int i = 0; i < n; i++)
		new_device(info + i);
	XIFreeDeviceInfo(info);

	if (!xi_devs.size()) {
		printf("Error: No suitable XInput devices found\n");
		exit(EXIT_FAILURE);
	}

	button_mask.deviceid = XIAllDevices;
	button_mask.mask = button_mask_data;
	button_mask.mask_len = sizeof(button_mask_data);
	memset(button_mask.mask, 0, button_mask.mask_len);
	XISetMask(button_mask.mask, XI_ButtonPress);
	XISetMask(button_mask.mask, XI_ButtonRelease);
	XISetMask(button_mask.mask, XI_Motion);

	touch_mask.deviceid = XIAllDevices;
	touch_mask.mask = touch_mask_data;
	touch_mask.mask_len = sizeof(touch_mask_data);
	memset(touch_mask.mask, 0, touch_mask.mask_len);
	XISetMask(touch_mask.mask, XI_TouchBegin);
	XISetMask(touch_mask.mask, XI_TouchUpdate);
	XISetMask(touch_mask.mask, XI_TouchEnd);

	raw_mask.deviceid = XIAllDevices;
	raw_mask.mask = raw_mask_data;
	raw_mask.mask_len = sizeof(raw_mask_data);
	memset(raw_mask.mask, 0, raw_mask.mask_len);
	XISetMask(raw_mask.mask, XI_ButtonPress);
	XISetMask(raw_mask.mask, XI_ButtonRelease);
	XISetMask(raw_mask.mask, XI_RawMotion);
	if (major > 2 || minor >= 2) {
		XISetMask(raw_mask.mask, XI_RawTouchBegin);
		XISetMask(raw_mask.mask, XI_RawTouchUpdate);
		XISetMask(raw_mask.mask, XI_RawTouchEnd);
	}

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
			xstate->ungrab(info->deviceid);
			xi_devs.erase(info->deviceid);
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
	for (DeviceMap::iterator i = xi_devs.begin(); i != xi_devs.end(); ++i) {
		i->second->active = !prefs.excluded_devices.ref().count(i->second->name);
		i->second->update_grabs();
	}
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

Grabber::XiDevice::XiDevice(Grabber *parent, XIDeviceInfo *info) :
	absolute(false),
	proximity_axis(-1),
	touch(false),
	scale_x(1.0),
	scale_y(1.0),
	num_buttons(0),
	button_grabbed(false),
	device_grabbed(GrabNo)
{
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
		} else if (dev_class->type == XITouchClass) {
			XITouchClassInfo *t = (XITouchClassInfo*)dev_class;
			if (t->mode == XIDirectTouch)
				touch = true;
		}
	}

	if (verbosity >= 1)
		printf("Opened Device %d ('%s'%s).\n", dev, info->name, absolute ? ": absolute" : "");

	update_grabs();
}

Grabber::XiDevice *Grabber::get_xi_dev(int id) {
	DeviceMap::iterator i = xi_devs.find(id);
	return i == xi_devs.end() ? NULL : i->second.get();
}

void Grabber::XiDevice::update_button(ButtonInfo &bi, bool grab) {
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
		XIGrabButton(dpy, dev, bi.button, ROOT, None, GrabModeAsync, GrabModeAsync, False, &button_mask, nmods, modifiers);
	else {
		XIUngrabButton(dpy, dev, bi.button, ROOT, nmods, modifiers);
//TODO		xstate->ungrab(dev);
	}
}

void Grabber::XiDevice::update_grabs() {
	bool grab_button  = active && grabber->grab_button && !(grabber->grab_touch && touch);

	// TODO: Take touch pref into account
	if (!grab_button != !button_grabbed)
		for (std::vector<ButtonInfo>::iterator j = grabber->buttons.begin(); j != grabber->buttons.end(); j++)
			update_button(*j, grab_button);
	if (grabber->grab_device == GrabNo && device_grabbed != GrabNo) {
		XIUngrabDevice(dpy, dev, CurrentTime);
//TODO		xstate->ungrab(dev);
	}
	if (grabber->grab_device != GrabNo && grabber->grab_device != device_grabbed)
		XIGrabDevice(dpy, dev, ROOT, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, grabber->grab_device == GrabYes ? &button_mask : &raw_mask);

	button_grabbed = grab_button;
	device_grabbed = grabber->grab_device;
}

void Grabber::update_grabs() {
	for (DeviceMap::iterator i = xi_devs.begin(); i != xi_devs.end(); ++i)
		i->second->update_grabs();

	XIGrabModifiers modifier = {0,};
	modifier.modifiers = XIAnyModifier;
	if (touch_grabbed == grab_touch)
		return;
	touch_grabbed = grab_touch;
	if (grab_touch)
		XIGrabTouchBegin(dpy, XIAllMasterDevices, ROOT, False, &touch_mask, 1, &modifier);
	else
		XIUngrabTouchBegin(dpy, XIAllMasterDevices, ROOT, 1, &modifier);
}

void Grabber::set() {
	switch (current) {
		case BUTTON:
		case GRAB:
			grab_button = !suspended && active && !disabled.get();
			grab_touch = !suspended && !disabled.get() && prefs.touch.get();
			break;
		case RAW:
			grab_button = !suspended;
			grab_touch = !suspended && prefs.touch.get();
			break;
		case SELECT:
			grab_button = false;
			grab_touch = false;
			break;
	}
	if (grab_button && current == GRAB)
		grab_device = GrabYes;
	else if (grab_button && current == RAW)
		grab_device = GrabRaw;
	else
		grab_device = GrabNo;

	if (verbosity >= 2)
		printf("Grabbing: %s (button: %d, touch: %d, device: %d)\n", state_name[current], grab_button, grab_touch, grab_device);

	update_grabs();

	bool old_select = select;
	select = !suspended && current == SELECT;
	if (old_select == select)
		return;
	if (select) {
		int code = XGrabPointer(dpy, ROOT, False, ButtonPressMask,
				GrabModeAsync, GrabModeAsync, ROOT, cursor_select, CurrentTime);
		if (code != GrabSuccess)
			throw GrabFailedException(code);
	} else {
		XUngrabPointer(dpy, CurrentTime);
	}
}

void Grabber::queue_suspend() {
	xstate->queue(sigc::mem_fun(*this, &Grabber::suspend));
}

void Grabber::queue_resume() {
	xstate->queue(sigc::mem_fun(*this, &Grabber::resume));
}

std::string Grabber::select_window() {
	return xstate->select_window();
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
	grab_button = false;
	update_grabs();

	grabbed_button = bi;
	buttons.clear();
	buttons.reserve(extra.size() + 1);
	buttons.push_back(bi);
	for (std::vector<ButtonInfo>::const_iterator i = extra.begin(); i != extra.end(); ++i)
		if (!i->overlap(bi))
			buttons.push_back(*i);
	set();
}

// Fuck Xlib
static bool has_wm_state(Window w) {
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
