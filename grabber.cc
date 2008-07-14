#include "grabber.h"
#include "main.h"
#include <X11/extensions/XTest.h>
#include <X11/Xutil.h>

bool no_xi = false;

const char *Grabber::state_name[5] = { "None", "Button", "All", "Xi_all", "Pointer" };

Grabber::Grabber() {
	current = BUTTON;
	suspended = false;
	active = true;
	grabbed = NONE;
	get_button();
	wm_state = XInternAtom(dpy, "WM_STATE", False);

	xinput = init_xi();
	grab_xi(true);

	init(ROOT, 0);
	cursor = XCreateFontCursor(dpy, XC_double_arrow);
}

Grabber::~Grabber() {
	XFreeCursor(dpy, cursor);
}

bool Grabber::init_xi() {
	xi_devs_n = 0;
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
		for (int j = 0; j < dev->num_classes; j++)
			if (dev->inputclassinfo[j].c_class == ButtonClass)
				has_button = true;

		if (!has_button)
			continue;

		if (verbosity >= 1)
			printf("Opening Device %s...\n", dev->name);

		XiDevice *xi_dev = new XiDevice;
		xi_dev->dev = XOpenDevice(dpy, dev->id);
		if (!xi_dev->dev) {
			printf("Opening Device %s failed.\n", dev->name);
			delete xi_dev;
			continue;
		}

		DeviceButtonPress(xi_dev->dev, xi_dev->button_down, xi_dev->button_events[0]);
		DeviceButtonRelease(xi_dev->dev, xi_dev->button_up, xi_dev->button_events[1]);
		DeviceButtonMotion(xi_dev->dev, xi_dev->button_motion, xi_dev->button_events[2]);
		XEventClass dummy;
		DeviceMotionNotify(xi_dev->dev, xi_dev->button_motion, dummy);
		xi_dev->button_events_n = 3;

		xi_devs[xi_devs_n++] = xi_dev;
	}

	return xi_devs_n;
}

bool Grabber::is_button_up(int type) {
	for (int i = 0; i < xi_devs_n; i++)
		if (type == xi_devs[i]->button_up)
			return true;
	return false;
}

bool Grabber::is_button_down(int type) {
	for (int i = 0; i < xi_devs_n; i++)
		if (type == xi_devs[i]->button_down)
			return true;
	return false;
}

bool Grabber::is_motion(int type) {
	for (int i = 0; i < xi_devs_n; i++)
		if (type == xi_devs[i]->button_motion)
			return true;
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
	if (grab) {
		for (int i = 0; i < xi_devs_n; i++)
			if (XGrabDeviceButton(dpy, xi_devs[i]->dev, button, state, NULL, ROOT, False,
						xi_devs[i]->button_events_n, xi_devs[i]->button_events,
						GrabModeAsync, GrabModeAsync))
				printf("Warning: Grabbing button %d on an xi device failed\n", button);

	} else for (int i = 0; i < xi_devs_n; i++)
		XUngrabDeviceButton(dpy, xi_devs[i]->dev, button, state, NULL, ROOT);
}

void Grabber::set() {
	State old = grabbed;
	grabbed = (!suspended && active) ? current : NONE;
	if (old == grabbed)
		return;
	if (verbosity >= 2)
		printf("grabbing: %s\n", state_name[grabbed]);

	if (old == BUTTON) {
		XUngrabButton(dpy, button, state, ROOT);
		if (timing_workaround)
			XUngrabButton(dpy, 1, state, ROOT);
	}
	if (old == ALL)
		XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
	if (old == XI_ALL)
		XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
	if (old == POINTER) {
		XUngrabPointer(dpy, CurrentTime);
	}
	if (grabbed == BUTTON) {
		while (!XGrabButton(dpy, button, state, ROOT, False, ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
					GrabModeSync, GrabModeAsync, None, None)) {
			printf("Error: Grab failed, retrying in 10 seconds...\n");
			sleep(10);
		}
		timing_workaround = button != 1 && prefs().timing_workaround.get();
		if (timing_workaround)
			XGrabButton(dpy, 1, state, ROOT, False, ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
					GrabModeSync, GrabModeAsync, None, None);
	}
	if (grabbed == ALL)
		if (!XGrabButton(dpy, AnyButton, AnyModifier, ROOT, False,
					ButtonPressMask, GrabModeSync, GrabModeAsync, None, None))
			throw GrabFailedException();
	if (grabbed == XI_ALL)
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
	button = ref->button;
	state = ref->state;
}

void Grabber::fake_button(int b) {
	suspend();
	XTestFakeButtonEvent(dpy, b, True, CurrentTime);
	XTestFakeButtonEvent(dpy, b, False, CurrentTime);
	clear_mods();
	resume();
}

void Grabber::ignore(int b) {
	XAllowEvents(dpy, ReplayPointer, CurrentTime);
	clear_mods();
	grab(ALL);
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
