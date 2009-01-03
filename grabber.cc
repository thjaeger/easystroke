/*
 * Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
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
#include "grabber.h"
#include "main.h"
#include <X11/extensions/XTest.h>
#include <X11/Xutil.h>
#include <glibmm/i18n.h>

bool no_xi = false;
Grabber *grabber = 0;

unsigned int ignore_mods[4] = { LockMask, Mod2Mask, LockMask | Mod2Mask, 0 };


const char* GrabFailedException::what() const throw() { return "Grab Failed"; }

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

extern Window get_window(Window w, Atom prop);

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

const char *Grabber::state_name[6] = { "None", "Button", "All (Sync)", "All (Async)", "Scroll", "Select" };

extern Window get_window(Window w, Atom prop);

bool wm_running() {
	static XAtom _NET_SUPPORTING_WM_CHECK("_NET_SUPPORTING_WM_CHECK");
	Window w = get_window(ROOT, *_NET_SUPPORTING_WM_CHECK);
	return w && get_window(w, *_NET_SUPPORTING_WM_CHECK) == w;
}

Grabber::Grabber() : children(ROOT) {
	current = BUTTON;
	suspended = false;
	xi_suspended = false;
	disabled = false;
	active = true;
	grabbed = NONE;
	xi_grabbed = false;
	xi_devs_grabbed = false;
	cursor_select = XCreateFontCursor(dpy, XC_crosshair);
	xinput = init_xi();
}

Grabber::~Grabber() {
	XFreeCursor(dpy, cursor_select);
}

float rescaleValuatorAxis(int coord, int fmin, int fmax, int tmax) {
	if (fmin >= fmax)
		return coord;
	return ((float)(coord - fmin)) * (tmax + 1) / (fmax - fmin + 1);
}

extern "C" {
	extern int _XiGetDevicePresenceNotifyEvent(Display *);
}

bool Grabber::init_xi() {
	button_events_n = 3;
	if (no_xi)
		return false;
	int nFEV, nFER;
	if (!XQueryExtension(dpy,INAME,&nMajor,&nFEV,&nFER))
		return false;

	// Macro not c++-safe
	// DevicePresence(dpy, event_presence, presence_class);
	event_presence = _XiGetDevicePresenceNotifyEvent(dpy);
	presence_class =  (0x10000 | _devicePresence);

	if (!update_device_list())
		return false;

	return xi_dev;
}

bool Grabber::update_device_list() {
	int n;
	XDeviceInfo *devs = XListInputDevices(dpy, &n);
	if (!devs)
		return false;

	xi_dev = 0;

	for (int i = 0; i < n; i++) {
		XDeviceInfo *dev = devs + i;

		if (strcmp(dev->name, "stylus"))
			continue;

		xi_dev = new XiDevice;

		bool has_button = false;
		xi_dev->supports_pressure = false;
		XAnyClassPtr any = (XAnyClassPtr) (dev->inputclassinfo);
		for (int j = 0; j < dev->num_classes; j++) {
			if (any->c_class == ButtonClass)
				has_button = true;
			if (any->c_class == ValuatorClass) {
				XValuatorInfo *info = (XValuatorInfo *)any;
				if (info->num_axes >= 2) {
					xi_dev->min_x = info->axes[0].min_value;
					xi_dev->max_x = info->axes[0].max_value;
					xi_dev->min_y = info->axes[1].min_value;
					xi_dev->max_y = info->axes[1].max_value;
				}
				if (info->num_axes >= 3) {
					xi_dev->supports_pressure = true;
					xi_dev->pressure_min = info->axes[2].min_value;
					xi_dev->pressure_max = info->axes[2].max_value;
				}
				xi_dev->absolute = info->mode == Absolute;
			}
			any = (XAnyClassPtr) ((char *) any + any->length);
		}

		if (!has_button) {
			delete xi_dev;
			continue;
		}

		xi_dev->dev = XOpenDevice(dpy, dev->id);
		if (!xi_dev->dev) {
			printf(_("Opening Device %s failed.\n"), dev->name);
			delete xi_dev;
			continue;
		}
		xi_dev->name = dev->name;

		DeviceButtonPress(xi_dev->dev, xi_dev->event_type[DOWN], xi_dev->events[DOWN]);
		DeviceButtonRelease(xi_dev->dev, xi_dev->event_type[UP], xi_dev->events[UP]);
		DeviceButtonMotion(xi_dev->dev, xi_dev->event_type[BUTTON_MOTION], xi_dev->events[BUTTON_MOTION]);
		DeviceMotionNotify(xi_dev->dev, xi_dev->event_type[MOTION], xi_dev->events[MOTION]);

		xi_dev->all_events_n = 4;
	}
	XFreeDeviceList(devs);
	xi_grabbed = false;
	set();
	return true;
}

Grabber::XiDevice *Grabber::get_xi_dev(XID id) {
	if (xi_dev->dev->device_id == id)
		return xi_dev;
	else
		return 0;
}

bool Grabber::is_event(int type, EventType et) {
	if (!xinput)
		return false;
	if (type == xi_dev->event_type[et])
		return true;
	return false;
}

void Grabber::grab_xi(bool grab) {
	if (!xinput)
		return;
	if (!xi_grabbed == !grab)
		return;
	xi_grabbed = grab;
	if (grab) {
		XGrabDeviceButton(dpy, xi_dev->dev, 1, 0, NULL,
				ROOT, False, button_events_n, xi_dev->events,
				GrabModeAsync, GrabModeAsync);
	} else {
		XUngrabDeviceButton(dpy, xi_dev->dev, 1, 0, NULL, ROOT);
	}
}

void Grabber::grab_xi_devs(bool grab) {
	if (!xi_devs_grabbed == !grab)
		return;
	xi_devs_grabbed = grab;
	if (grab) {
		if (XGrabDevice(dpy, xi_dev->dev, ROOT, False,
					xi_dev->all_events_n,
					xi_dev->events, GrabModeAsync, GrabModeAsync, CurrentTime))
			throw GrabFailedException();
	} else
		XUngrabDevice(dpy, xi_dev->dev, CurrentTime);
}

void Grabber::set() {
	bool act = (current == NONE || current == BUTTON) ? active && !disabled : true;
	grab_xi(!xi_suspended && current != ALL_SYNC && act);
	grab_xi_devs(!xi_suspended && current == NONE && act);
	State old = grabbed;
	grabbed = (!suspended && act) ? current : NONE;
	if (old == grabbed)
		return;
	if (verbosity >= 2)
		printf("grabbing: %s\n", state_name[grabbed]);

	if (old == BUTTON) {
		XUngrabButton(dpy, 1, 0, ROOT);
	}
	if (grabbed == BUTTON) {
		XGrabButton(dpy, 1, 0, ROOT, False,
				ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
					GrabModeSync, GrabModeAsync, None, None);
	}
}

bool Grabber::is_grabbed(guint b) {
	return b == 1;
}

bool Grabber::is_instant(guint b) {
	return false;
}

int get_default_button() {
	return 1;
}

void Grabber::update(Window w) {
	wm_class = get_wm_class(w);
	active = true;
	set();
}

void Grabber::XiDevice::fake_press(int b, int core) {
	XTestFakeDeviceButtonEvent(dpy, dev, b, True,  0, 0, 0);
}
void Grabber::XiDevice::fake_release(int b, int core) {
	XTestFakeDeviceButtonEvent(dpy, dev, b, False, 0, 0, 0);
}

std::string Grabber::get_wm_class(Window w) {
	if (!w)
		return _("(window manager frame)");
	XClassHint ch;
	if (!XGetClassHint(dpy, w, &ch))
		return "";
	std::string ans = ch.res_name;
	XFree(ch.res_name);
	XFree(ch.res_class);
	return ans;
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

// sets w to 0 if the window is a frame
Window get_app_window(Window &w) {
	return w;
}
