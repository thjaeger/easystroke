#include "grabber.h"
#include "main.h"
#include <X11/extensions/XTest.h>
#include <X11/Xutil.h>

bool no_xi = false;

const char *Grabber::state_name[5] = { "None", "Button", "All (Sync)", "All (Async)", "Pointer" };

Grabber::Grabber() {
	current = BUTTON;
	suspended = false;
	active = true;
	grabbed = NONE;
	xi_grabbed = false;
	get_button();
	wm_state = XInternAtom(dpy, "WM_STATE", False);

	xinput = init_xi();
	if (xinput)
		select_proximity();

	init(ROOT, 0);
	cursor = XCreateFontCursor(dpy, XC_double_arrow);
}

Grabber::~Grabber() {
	XFreeCursor(dpy, cursor);
}

bool Grabber::init_xi() {
	xi_devs_n = 0;
	button_events_n = 3;
	all_events_n = 4;
	proximity_events_n = 2;
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

		bool has_button = false;
		bool supports_pressure = false;
		int pressure_min = 0, pressure_max = 0;
		XAnyClassPtr any = (XAnyClassPtr) (dev->inputclassinfo);
		for (int j = 0; j < dev->num_classes; j++) {
			if (any->c_class == ButtonClass)
				has_button = true;
			if (any->c_class == ValuatorClass) {
				XValuatorInfo *info = (XValuatorInfo *)any;
				if (info->num_axes >= 3)
					supports_pressure = true;
				pressure_min = info->axes[2].min_value;
				pressure_max = info->axes[2].max_value;
			}
			any = (XAnyClassPtr) ((char *) any + any->length);
		}

		if (!has_button)
			continue;

		XiDevice *xi_dev = new XiDevice;
		xi_dev->dev = XOpenDevice(dpy, dev->id);
		if (!xi_dev->dev) {
			printf("Opening Device %s failed.\n", dev->name);
			delete xi_dev;
			continue;
		}

		DeviceButtonPress(xi_dev->dev, xi_dev->event_type[DOWN], xi_dev->events[0]);
		DeviceButtonRelease(xi_dev->dev, xi_dev->event_type[UP], xi_dev->events[1]);
		DeviceButtonMotion(xi_dev->dev, xi_dev->event_type[MOTION], xi_dev->events[2]);
		DeviceMotionNotify(xi_dev->dev, xi_dev->event_type[MOTION], xi_dev->events[3]);

		ProximityIn(xi_dev->dev, xi_dev->event_type[PROX_IN], xi_dev->events[4]);
		ProximityOut(xi_dev->dev, xi_dev->event_type[PROX_OUT], xi_dev->events[5]);
		xi_dev->supports_proximity = xi_dev->events[4] && xi_dev->events[5];

		xi_dev->supports_pressure = supports_pressure;
		xi_dev->pressure_min = pressure_min;
		xi_dev->pressure_max = pressure_max;

		xi_devs[xi_devs_n++] = xi_dev;

		if (verbosity >= 1)
			printf("Opened Device \"%s\" (%s proximity).\n", dev->name,
					xi_dev->supports_proximity ? "supports" : "does not support");
	}

	return xi_devs_n;
}

bool Grabber::supports_pressure() {
	for (int i = 0; i < xi_devs_n; i++)
		if (xi_devs[i]->supports_pressure)
			return true;
	return false;
}

bool Grabber::is_event(int type, EventType et, XDevice **dev) {
	if (!xinput)
		return false;
	for (int i = 0; i < xi_devs_n; i++)
		if (type == xi_devs[i]->event_type[et]) {
			if (dev)
				*dev = xi_devs[i]->dev;
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
	if (Success != XGetWindowProperty(dpy, w, wm_state, 0, 2, False,
				AnyPropertyType, &actual_type_return,
				&actual_format_return, &nitems_return,
				&bytes_after_return, &prop_return))
		return false;
	XFree(prop_return);
	return nitems_return;
}

// Calls create on top-level windows, i.e. windows that have th wm_state property.
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
		if (!has_wm_state(ch[i]))
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
						all_events_n + (xi_devs[i]->supports_proximity ? proximity_events_n : 0),
						xi_devs[i]->events, GrabModeAsync, GrabModeAsync, CurrentTime))
				throw GrabFailedException();
	} else
		for (int i = 0; i < xi_devs_n; i++)
			XUngrabDevice(dpy, xi_devs[i]->dev, CurrentTime);
}

void Grabber::select_proximity() {
	for (int i = 0; i < xi_devs_n; i++)
		if (xi_devs[i]->supports_proximity)
			XSelectExtensionEvent(dpy, ROOT, xi_devs[i]->events + all_events_n, proximity_events_n);
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
		timing_workaround = !is_grabbed(1) && prefs().timing_workaround.get();
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
	Ref<ButtonInfo> ref(prefs().button);
	buttons.clear();
	buttons[ref->button] = ref->state;
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
	XSetWindowAttributes attr;
	attr.event_mask = EnterWindowMask;
	XChangeWindowAttributes(dpy, w, CWEventMask, &attr);
}
