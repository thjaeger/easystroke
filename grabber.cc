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
#include "grabber.h"
#include "main.h"
#include <X11/extensions/XTest.h>
#include <X11/Xutil.h>
#include <glibmm/i18n.h>

bool no_xi = false;
bool xi_15 = false;
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

void activate(Window w, Time t) {
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

void Grabber::unminimize() {
	if (minimized.empty())
		return;
	Window w;
	unsigned int n;
	minimized.pop(n, w);
	activate(w, CurrentTime);
}

extern Source<bool> xinput_v, supports_pressure, supports_proximity;

const char *Grabber::state_name[6] = { "None", "Button", "All (Sync)", "All (Async)", "Scroll", "Select" };

extern Window get_window(Window w, Atom prop);

Grabber::Grabber() : children(ROOT) {
	current = BUTTON;
	suspended = false;
	disabled = false;
	active = true;
	grabbed = NONE;
	xi_grabbed = false;
	xi_devs_grabbed = false;
	proximity_selected = false;
	grabbed_button.button = 0;
	grabbed_button.state = 0;
	cursor_select = XCreateFontCursor(dpy, XC_crosshair);
	xinput = init_xi();
	update_button(prefs.button.get());
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

#ifndef XI_Add_DeviceProperties_Major
#define XI_Add_DeviceProperties_Major 1
#endif

#ifndef XI_Add_DeviceProperties_Minor
#define XI_Add_DeviceProperties_Minor 5
#endif

bool Grabber::init_xi() {
	xi_devs = 0;
	button_events_n = 3;
	if (no_xi)
		return false;
	int nFEV, nFER;
	if (!XQueryExtension(dpy,INAME,&nMajor,&nFEV,&nFER))
		return false;
	XExtensionVersion *v = XGetExtensionVersion(dpy, INAME);
	if (!v->present)
		return false;
	xi_15 = v->major_version > XI_Add_DeviceProperties_Major ||
		(v->major_version == XI_Add_DeviceProperties_Major && v->minor_version >= XI_Add_DeviceProperties_Minor);
	XFree(v);

	// Macro not c++-safe
	// DevicePresence(dpy, event_presence, presence_class);
	event_presence = _XiGetDevicePresenceNotifyEvent(dpy);
	presence_class =  (0x10000 | _devicePresence);

	if (!update_device_list())
		return false;

	xinput_v.set(xi_devs_n);

	for (int i = 0; i < xi_devs_n; i++)
		if (xi_devs[i]->supports_pressure) {
			supports_pressure.set(true);
			break;
		}

	for (int i = 0; i < xi_devs_n; i++)
		if (xi_devs[i]->supports_proximity) {
			supports_proximity.set(true);
			break;
		}
	prefs.proximity.connect(new Notifier(sigc::mem_fun(*this, &Grabber::select_proximity)));

	return xi_devs_n;
}

bool Grabber::update_device_list() {
	if (xi_devs) {
		for (int i = 0; i < xi_devs_n; i++)
			XCloseDevice(dpy, xi_devs[i]->dev);
		delete[] xi_devs;
	}

	int n;
	XDeviceInfo *devs = XListInputDevices(dpy, &n);
	if (!devs)
		return false;

	xi_devs = new XiDevice *[n];
	xi_devs_n = 0;

	for (int i = 0; i < n; i++) {
		XDeviceInfo *dev = devs + i;

		if (dev->use == IsXKeyboard || dev->use == IsXPointer)
			continue;

		XiDevice *xi_dev = new XiDevice;

		xi_dev->num_buttons = 0;
		xi_dev->supports_pressure = false;
		XAnyClassPtr any = (XAnyClassPtr) (dev->inputclassinfo);
		for (int j = 0; j < dev->num_classes; j++) {
			if (any->c_class == ButtonClass) {
				XButtonInfo *info = (XButtonInfo *)any;
				xi_dev->num_buttons = info->num_buttons;
			}
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

		if (!xi_dev->num_buttons) {
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

		xi_dev->valuators[0] = 0;
		xi_dev->valuators[1] = 0;

		DeviceButtonPress(xi_dev->dev, event_type[DOWN], xi_dev->events[DOWN]);
		DeviceButtonRelease(xi_dev->dev, event_type[UP], xi_dev->events[UP]);
		DeviceButtonMotion(xi_dev->dev, event_type[BUTTON_MOTION], xi_dev->events[BUTTON_MOTION]);
		DeviceMotionNotify(xi_dev->dev, event_type[MOTION], xi_dev->events[MOTION]);

		int prox_in, prox_out;
		ProximityIn(xi_dev->dev, prox_in, xi_dev->events[PROX_IN]);
		ProximityOut(xi_dev->dev, prox_out, xi_dev->events[PROX_OUT]);
		xi_dev->supports_proximity = xi_dev->events[PROX_IN] && xi_dev->events[PROX_OUT];
		if (xi_dev->supports_proximity) {
			event_type[PROX_IN] = prox_in;
			event_type[PROX_OUT] = prox_out;
		}
		xi_dev->all_events_n = xi_dev->supports_proximity ? 6 : 4;

		xi_devs[xi_devs_n++] = xi_dev;

		XEventClass evs[0];
		DeviceMappingNotify(xi_dev->dev, mapping_notify, evs[0]);
		XSelectExtensionEvent(dpy, ROOT, evs, 1);
		xi_dev->update_pointer_mapping();

		if (verbosity >= 1)
			printf(_("Opened Device \"%s\" (%s, %s proximity).\n"), dev->name,
					xi_dev->absolute ? _("absolute") : _("relative"),
					xi_dev->supports_proximity ? _("supports") : _("does not support"));
	}
	XFreeDeviceList(devs);
	proximity_selected = false;
	select_proximity();
	xi_grabbed = false;
	set();
	return true;
}

Grabber::XiDevice *Grabber::get_xi_dev(XID id) {
	for (int i = 0; i < xi_devs_n; i++)
		if (xi_devs[i]->dev->device_id == id)
			return xi_devs[i];
	return 0;
}

unsigned int Grabber::get_device_button_state(XiDevice *&dev) {
	unsigned int mask = 0;
	for (int i = 0; i < xi_devs_n; i++) {
		if (prefs.excluded_devices.get().count(xi_devs[i]->name))
			continue;
		XDeviceState *state = XQueryDeviceState(dpy, xi_devs[i]->dev);
		if (!state)
			continue;
		XInputClass *c = state->data;
		for (int j = 0; j < state->num_classes; j++) {
			if (c->c_class == ButtonClass) {
				XButtonState *b = (XButtonState *)c;
				mask |= b->buttons[0];
				mask |= ((unsigned int)b->buttons[1]) << 8;
				mask |= ((unsigned int)b->buttons[2]) << 16;
				mask |= ((unsigned int)b->buttons[3]) << 24;
			}
			c = (XInputClass *)((char *)c + c->length);
		}
		XFreeDeviceState(state);
		if (mask) {
			dev = xi_devs[i];
			return mask;
		}
	}
	return 0;
}

void Grabber::grab_xi(bool grab) {
	if (!xinput)
		return;
	if (!xi_grabbed == !grab)
		return;
	xi_grabbed = grab;
	for (int i = 0; i < xi_devs_n; i++)
		if (!prefs.excluded_devices.get().count(xi_devs[i]->name)) {
			if (grab) {
				for (std::vector<ButtonInfo>::iterator j = buttons.begin(); j != buttons.end(); j++) {
					XGrabDeviceButton(dpy, xi_devs[i]->dev, j->button, j->state, NULL,
							ROOT, False, button_events_n, xi_devs[i]->events,
							GrabModeAsync, GrabModeAsync);
					if (j->state == AnyModifier)
						continue;
					for (unsigned int *mask = ignore_mods; *mask; mask++)
						XGrabDeviceButton(dpy, xi_devs[i]->dev, j->button, j->state ^ *mask,
								NULL, ROOT, False, button_events_n, xi_devs[i]->events,
								GrabModeAsync, GrabModeAsync);
				}

			} else
				for (std::vector<ButtonInfo>::iterator j = buttons.begin(); j != buttons.end(); j++) {
					XUngrabDeviceButton(dpy, xi_devs[i]->dev, j->button, j->state, NULL, ROOT);
					if (j->state == AnyModifier)
						continue;
					for (unsigned int *mask = ignore_mods; *mask; mask++)
						XUngrabDeviceButton(dpy, xi_devs[i]->dev, j->button,
								j->state ^ *mask, NULL, ROOT);
				}
		}
}

void Grabber::XiDevice::grab_device(bool grab) {
	if (grab) {
		if (XGrabDevice(dpy, dev, ROOT, False, all_events_n, events, GrabModeAsync, GrabModeAsync, CurrentTime))
			throw GrabFailedException();
	} else
		XUngrabDevice(dpy, dev, CurrentTime);
}

void Grabber::grab_xi_devs(bool grab) {
	if (!xi_devs_grabbed == !grab)
		return;
	xi_devs_grabbed = grab;
	for (int i = 0; i < xi_devs_n; i++)
		xi_devs[i]->grab_device(grab);
}

void Grabber::XiDevice::update_pointer_mapping() {
	if (!xi_15)
		return;
#define MAX_BUTTONS 256
	unsigned char map[MAX_BUTTONS];
	int n = XGetDeviceButtonMapping(dpy, dev, map, MAX_BUTTONS);
	inv_map.clear();
	for (int i = n-1; i; i--)
		if (map[i] == i+1)
			inv_map.erase(i+1);
		else
			inv_map[map[i]] = i+1;
}

extern bool in_proximity;
void Grabber::select_proximity() {
	bool select = prefs.proximity.get();
	if (!select != !proximity_selected) {
		proximity_selected = !proximity_selected;
		if (!proximity_selected)
			in_proximity = false;
	}
	XEventClass evs[2*xi_devs_n+1];
	int n = 0;
	evs[n++] = presence_class;
	for (int i = 0; i < xi_devs_n; i++) {
		if (proximity_selected && xi_devs[i]->supports_proximity) {
			evs[n++] = xi_devs[i]->events[PROX_IN];
			evs[n++] = xi_devs[i]->events[PROX_OUT];
		}
	}
	// NB: Unselecting doesn't actually work!
	XSelectExtensionEvent(dpy, ROOT, evs, n);
}

void Grabber::set() {
	bool act = !suspended && ((active && !disabled) || (current != NONE && current != BUTTON));
	grab_xi(act && current != ALL_SYNC);
	grab_xi_devs(act && current == NONE);
	State old = grabbed;
	grabbed = act ? current : NONE;
	if (old == grabbed)
		return;
	if (verbosity >= 2)
		printf("grabbing: %s\n", state_name[grabbed]);

	if (old == BUTTON) {
		for (std::vector<ButtonInfo>::iterator j = buttons.begin(); j != buttons.end(); j++) {
			XUngrabButton(dpy, j->button, j->state, ROOT);
			if (j->state == AnyModifier)
				continue;
			for (unsigned int *mask = ignore_mods; *mask; mask++)
				XUngrabButton(dpy, j->button, j->state ^ *mask, ROOT);
		}
		if (timing_workaround)
			XUngrabButton(dpy, 1, AnyModifier, ROOT);
	}
	if (old == ALL_SYNC)
		XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
	if (old == SELECT)
		XUngrabPointer(dpy, CurrentTime);

	if (grabbed == BUTTON) {
		unsigned int core_mask = xinput ? ButtonPressMask : ButtonMotionMask | ButtonPressMask | ButtonReleaseMask;
		for (std::vector<ButtonInfo>::iterator j = buttons.begin(); j != buttons.end(); j++) {
			XGrabButton(dpy, j->button, j->state, ROOT, False, core_mask,
					xinput ? GrabModeSync : GrabModeAsync, GrabModeAsync, None, None);
			if (j->state == AnyModifier)
				continue;
			for (unsigned int *mask = ignore_mods; *mask; mask++)
				XGrabButton(dpy, j->button, j->state ^ *mask, ROOT, False, core_mask,
						GrabModeSync, GrabModeAsync, None, None);
		}
		timing_workaround = xinput && !is_grabbed(1) && prefs.timing_workaround.get();
		if (timing_workaround)
			XGrabButton(dpy, 1, AnyModifier, ROOT, False, ButtonPressMask,
					GrabModeSync, GrabModeAsync, None, None);
	}
	if (grabbed == ALL_SYNC)
		XGrabButton(dpy, AnyButton, AnyModifier, ROOT, False, ButtonPressMask,
				GrabModeSync, GrabModeAsync, None, None);
	if (grabbed == SELECT) {
		int i = 0;
		while (XGrabPointer(dpy, ROOT, False, ButtonPressMask,
					GrabModeAsync, GrabModeAsync, ROOT, cursor_select, CurrentTime) != GrabSuccess) {
			if (++i > 10)
				throw GrabFailedException();
			usleep(10000);
		}
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

void Grabber::update_button(ButtonInfo bi) {
	const std::vector<ButtonInfo> &extra = prefs.extra_buttons.ref();
	if (grabbed_button == bi && buttons.size() == extra.size() + 1 &&
			std::equal(extra.begin(), extra.end(), ++buttons.begin()))
		return;
	suspended = true;
	set();
	grabbed_button = bi;
	buttons.clear();
	buttons.reserve(extra.size() + 1);
	buttons.push_back(bi);
	for (std::vector<ButtonInfo>::const_iterator i = extra.begin(); i != extra.end(); ++i)
		if (!i->overlap(bi))
			buttons.push_back(*i);
	suspended = false;
	set();
}

int get_default_button() {
	if (grabber)
		return grabber->get_default_button();
	else
		return prefs.button.get().button;
}

void Grabber::update(Window w) {
	wm_class = get_wm_class(w);
	std::map<std::string, RButtonInfo>::const_iterator i = prefs.exceptions.ref().find(wm_class);
	active = true;
	ButtonInfo bi = prefs.button.ref();
	if (i != prefs.exceptions.ref().end()) {
		if (i->second) {
			bi = *i->second;
		} else {
			active = false;
		}
	}
	update_button(bi);
	set();
}

void Grabber::release_all(int n) {
	if (xi_15) {
		for (int i = 0; i < xi_devs_n; i++)
			xi_devs[i]->release_all();
	} else {
		if (!n)
			n = XGetPointerMapping(dpy, 0, 0);
		for (int i = 1; i <= n; i++)
			XTestFakeButtonEvent(dpy, i, False, CurrentTime);
	}

}

void Grabber::XiDevice::release_all() {
	for (int i = 1; i <= num_buttons; i++)
		XTestFakeDeviceButtonEvent(dpy, dev, i, False, valuators, 2, 0);
}

void Grabber::XiDevice::fake_button(int b, bool press) {
	guint b2 = b;
	std::map<guint, guint>::iterator i = inv_map.find(b);
	if (i != inv_map.end())
		b2 = i->second;
	// we actually do need to pass valuators here, otherwise all hell will break lose
	XTestFakeDeviceButtonEvent(dpy, dev, b2, press, valuators, 2, 0);
	if (verbosity >= 3)
		printf("fake xi %s: %d -> %d\n", press ? "press" : "release", b2, b);
	if (!xi_15)
		XTestFakeButtonEvent(dpy, b2, press, CurrentTime);
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
	if (!w)
		return w;
	if (frame_win.contains1(w)) {
		Window w2 = frame_win.find1(w);
		w = 0;
		return w2;
	}
	if (frame_child.contains1(w)) {
		Window w2 = frame_child.find1(w);
		if (w != w2)
			w = 0;
		return w2;
	}
	Window w2 = find_wm_state(w);
	if (w2) {
		frame_child.add(w, w2);
		if (w2 != w) {
			w = w2;
			XSelectInput(dpy, w2, EnterWindowMask | LeaveWindowMask | StructureNotifyMask | PropertyChangeMask);
		}
		return w2;
	}
	if (verbosity >= 1)
		printf(_("Window 0x%lx does not have an associated top-level window\n"), w);
	return w;
}
