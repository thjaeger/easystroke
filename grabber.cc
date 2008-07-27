#include "grabber.h"
#include "main.h"
#include <X11/extensions/XTest.h>
#include <X11/Xutil.h>

bool no_xi = false;
Grabber *grabber = 0;

const char *Grabber::state_name[5] = { "None", "Button", "All (Sync)", "All (Async)", "Pointer" };

extern Window get_window(Window w, Atom prop);

bool wm_running() {
	Atom _NET_SUPPORTING_WM_CHECK = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	Window w = get_window(ROOT, _NET_SUPPORTING_WM_CHECK);
	return w && get_window(w, _NET_SUPPORTING_WM_CHECK) == w;
}

void reinit() {
	sleep(15);
	send(P_SCAN_WINDOWS);
}

void Grabber::scan_windows() {
	init(ROOT, wm_running() ? 0 : -1);
}

Grabber::Grabber() {
	current = BUTTON;
	suspended = false;
	active = true;
	grabbed = NONE;
	xi_grabbed = false;
	proximity_selected = false;
	get_button();
	WM_STATE = XInternAtom(dpy, "WM_STATE", False);

	xinput = init_xi();
	if (xinput)
		select_proximity();

	XSelectInput(dpy, ROOT, SubstructureNotifyMask);
	scan_windows();
	Glib::Thread::create(sigc::ptr_fun(&reinit), false);

	cursor = XCreateFontCursor(dpy, XC_double_arrow);
}

Grabber::~Grabber() {
	XFreeCursor(dpy, cursor);
}

extern "C" {
	extern int _XiGetDevicePresenceNotifyEvent(Display *);
}

bool Grabber::init_xi() {
	xi_devs_n = 0;
	button_events_n = 3;
	all_events_n = 4;
	if (no_xi)
		return false;
	int nMajor, nFEV, nFER;
	if (!XQueryExtension(dpy,INAME,&nMajor,&nFEV,&nFER))
		return false;

	int i, n;
	XDeviceInfo *devs;
	devs = XListInputDevices(dpy, &n);
	if (!devs)
		return false;

	xi_devs = new XiDevice *[n];

	for (i=0; i<n; ++i) {
		XDeviceInfo *dev = devs + i;

		if (dev->use == IsXKeyboard || dev->use == IsXPointer)
			continue;

		XiDevice *xi_dev = new XiDevice;

		bool has_button = false;
		xi_dev->supports_pressure = false;
		XAnyClassPtr any = (XAnyClassPtr) (dev->inputclassinfo);
		for (int j = 0; j < dev->num_classes; j++) {
			if (any->c_class == ButtonClass)
				has_button = true;
			if (any->c_class == ValuatorClass) {
				XValuatorInfo *info = (XValuatorInfo *)any;
				if (info->num_axes >= 3) {
					xi_dev->supports_pressure = true;
					xi_dev->pressure_min = info->axes[2].min_value;
					xi_dev->pressure_max = info->axes[2].max_value;
				}
			}
			any = (XAnyClassPtr) ((char *) any + any->length);
		}

		if (!has_button) {
			delete xi_dev;
			continue;
		}

		xi_dev->dev = XOpenDevice(dpy, dev->id);
		if (!xi_dev->dev) {
			printf("Opening Device %s failed.\n", dev->name);
			delete xi_dev;
			continue;
		}

		DeviceButtonPress(xi_dev->dev, xi_dev->event_type[DOWN], xi_dev->events[DOWN]);
		DeviceButtonRelease(xi_dev->dev, xi_dev->event_type[UP], xi_dev->events[UP]);
		DeviceButtonMotion(xi_dev->dev, xi_dev->event_type[BUTTON_MOTION], xi_dev->events[BUTTON_MOTION]);
		DeviceMotionNotify(xi_dev->dev, xi_dev->event_type[MOTION], xi_dev->events[MOTION]);

		ChangeDeviceNotify(xi_dev->dev, xi_dev->event_type[CHANGE], xi_dev->events[CHANGE]);
		ProximityIn(xi_dev->dev, xi_dev->event_type[PROX_IN], xi_dev->events[PROX_IN]);
		ProximityOut(xi_dev->dev, xi_dev->event_type[PROX_OUT], xi_dev->events[PROX_OUT]);
		xi_dev->supports_proximity = xi_dev->events[PROX_IN] && xi_dev->events[PROX_OUT];
		xi_dev->all_events_n = xi_dev->supports_proximity ? 7 : 5;

		xi_devs[xi_devs_n++] = xi_dev;

		if (verbosity >= 1)
			printf("Opened Device \"%s\" (%s proximity).\n", dev->name,
					xi_dev->supports_proximity ? "supports" : "does not support");
	}
	XFreeDeviceList(devs);

	// Macro not c++-safe
	// DevicePresence(dpy, event_presence, presence_class);
	event_presence = _XiGetDevicePresenceNotifyEvent(dpy);
	presence_class =  (0x10000 | _devicePresence);

	return xi_devs_n;
}

Grabber::XiDevice *Grabber::get_xi_dev(XID id) {
	for (int i = 0; i < xi_devs_n; i++)
		if (xi_devs[i]->dev->device_id == id)
			return xi_devs[i];
	return 0;
}

bool Grabber::supports_pressure() {
	for (int i = 0; i < xi_devs_n; i++)
		if (xi_devs[i]->supports_pressure)
			return true;
	return false;
}

bool Grabber::supports_proximity() {
	for (int i = 0; i < xi_devs_n; i++)
		if (xi_devs[i]->supports_proximity)
			return true;
	return false;
}

bool Grabber::is_event(int type, EventType et) {
	if (!xinput)
		return false;
	for (int i = 0; i < xi_devs_n; i++)
		if (type == xi_devs[i]->event_type[et]) {
			return true;
		}
	return false;
}

unsigned int Grabber::get_device_button_state() {
	unsigned int mask = 0;
	for (int i = 0; i < xi_devs_n; i++) {
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
	}

	return mask;
}

// Fuck Xlib
bool Grabber::has_wm_state(Window w) {
	Atom actual_type_return;
	int actual_format_return;
	unsigned long nitems_return;
	unsigned long bytes_after_return;
	unsigned char *prop_return;
	if (Success != XGetWindowProperty(dpy, w, WM_STATE, 0, 2, False,
				AnyPropertyType, &actual_type_return,
				&actual_format_return, &nitems_return,
				&bytes_after_return, &prop_return))
		return false;
	XFree(prop_return);
	return nitems_return;
}

// Calls create on top-level windows, i.e.
//   if depth >= 0: windows that have th wm_state property
//   if depth <  0: children of the root window
void Grabber::init(Window w, int depth) {
	depth++;
	// I have no clue why this is needed, but just querying the whole tree
	// prevents EnterNotifies from being generated.  Weird.
	// update 2/12/08:  Disappeared.  Leaving the code here in case this
	// comes back
	// 2/15/08: put it back in for release.  You never know.
	if (depth > 2)
		return;
	unsigned int n;
	Window dummyw1, dummyw2, *ch;
	XQueryTree(dpy, w, &dummyw1, &dummyw2, &ch, &n);
	for (unsigned int i = 0; i != n; i++) {
		if (depth > 0 && !has_wm_state(ch[i]))
			init(ch[i], depth);
		else
			create(ch[i]);
	}
	XFree(ch);
}

void Grabber::grab_xi(bool grab) {
	if (!xi_grabbed == !grab)
		return;
	xi_grabbed = grab;
	if (grab) {
		for (int i = 0; i < xi_devs_n; i++)
			for (std::map<guint, guint>::iterator j = buttons.begin(); j != buttons.end(); j++) {
				XGrabDeviceButton(dpy, xi_devs[i]->dev, j->first, j->second, NULL, ROOT, False,
							button_events_n, xi_devs[i]->events,
							GrabModeAsync, GrabModeAsync);
				XGrabDeviceButton(dpy, xi_devs[i]->dev, j->first, j->second ^ Mod2Mask, NULL, ROOT, False,
							button_events_n, xi_devs[i]->events,
							GrabModeAsync, GrabModeAsync);
			}

	} else for (int i = 0; i < xi_devs_n; i++)
		for (std::map<guint, guint>::iterator j = buttons.begin(); j != buttons.end(); j++) {
			XUngrabDeviceButton(dpy, xi_devs[i]->dev, j->first, j->second ^ Mod2Mask, NULL, ROOT);
			XUngrabDeviceButton(dpy, xi_devs[i]->dev, j->first, j->second, NULL, ROOT);
		}
}

void Grabber::grab_xi_devs(bool grab) {
	if (grab) {
		for (int i = 0; i < xi_devs_n; i++)
			if (XGrabDevice(dpy, xi_devs[i]->dev, ROOT, False,
						xi_devs[i]->all_events_n,
						xi_devs[i]->events, GrabModeAsync, GrabModeAsync, CurrentTime))
				throw GrabFailedException();
	} else
		for (int i = 0; i < xi_devs_n; i++)
			XUngrabDevice(dpy, xi_devs[i]->dev, CurrentTime);
}

extern bool in_proximity;
void Grabber::select_proximity() {
	if (!prefs().proximity.get() == !proximity_selected)
		return;
	proximity_selected = !proximity_selected;
	if (!proximity_selected)
		in_proximity = false;
	for (int i = 0; i < xi_devs_n; i++)
		if (xi_devs[i]->supports_proximity)
			if (proximity_selected)
				XSelectExtensionEvent(dpy, ROOT, xi_devs[i]->events + all_events_n, proximity_events_n);
			else // NB: This is total BS. The following call doesn't actually unselect the events.
				XSelectExtensionEvent(dpy, ROOT, 0, 0);
}

void Grabber::set() {
	grab_xi(active);
	State old = grabbed;
	grabbed = (!suspended && active) ? current : NONE;
	if (old == grabbed)
		return;
	if (verbosity >= 2)
		printf("grabbing: %s\n", state_name[grabbed]);

	if (old == BUTTON) {
		for (std::map<guint, guint>::iterator j = buttons.begin(); j != buttons.end(); j++) {
			XUngrabButton(dpy, j->first, j->second ^ Mod2Mask, ROOT);
			XUngrabButton(dpy, j->first, j->second, ROOT);
		}
		if (timing_workaround)
			XUngrabButton(dpy, 1, AnyModifier, ROOT);
	}
	if (old == ALL_SYNC)
		XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
	if (old == ALL_ASYNC)
		XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
	if (old == POINTER) {
		XUngrabPointer(dpy, CurrentTime);
	}
	if (grabbed == BUTTON) {
		for (std::map<guint, guint>::iterator j = buttons.begin(); j != buttons.end(); j++) {
			XGrabButton(dpy, j->first, j->second, ROOT, False, 
					ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
					GrabModeSync, GrabModeAsync, None, None);
			XGrabButton(dpy, j->first, j->second ^ Mod2Mask, ROOT, False, 
					ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
					GrabModeSync, GrabModeAsync, None, None);
		}
		timing_workaround = !is_grabbed(1) && prefs.timing_workaround.get();
		if (timing_workaround)
			XGrabButton(dpy, 1, AnyModifier, ROOT, False, ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
					GrabModeSync, GrabModeAsync, None, None);
	}
	if (grabbed == ALL_SYNC)
		if (!XGrabButton(dpy, AnyButton, AnyModifier, ROOT, False,
					ButtonPressMask, GrabModeSync, GrabModeAsync, None, None))
			throw GrabFailedException();
	if (grabbed == ALL_ASYNC)
		XGrabButton(dpy, AnyButton, AnyModifier, ROOT, True, ButtonPressMask,
				GrabModeAsync, GrabModeAsync, None, None);
	if (grabbed == POINTER) {
		int i = 0;
		while (XGrabPointer(dpy, ROOT, False, PointerMotionMask|ButtonMotionMask|ButtonPressMask|ButtonReleaseMask,
					GrabModeAsync, GrabModeAsync, ROOT, cursor, CurrentTime) != GrabSuccess) {
			if (++i > 10)
				throw GrabFailedException();
			usleep(10000);
		}
	}
}

void Grabber::get_button() {
	Setter s;
	ButtonInfo &bi = s.ref(prefs.button);
	buttons.clear();
	buttons[bi.button] = bi.state;
}

void Grabber::fake_button(int b) {
	suspend();
	XTestFakeButtonEvent(dpy, b, True, CurrentTime);
	XTestFakeButtonEvent(dpy, b, False, CurrentTime);
	clear_mods();
	resume();
}

std::string Grabber::get_wm_state(Window w) {
	XClassHint ch;
	if (!XGetClassHint(dpy, w, &ch))
		return "";
	std::string ans = ch.res_name;
	XFree(ch.res_name);
	XFree(ch.res_class);
	return ans;
}

void Grabber::create(Window w) {
	if (!w)
		return;
	XSelectInput(dpy, w, EnterWindowMask);
}
