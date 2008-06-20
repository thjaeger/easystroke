#include "grabber.h"
#include "main.h"
#include <X11/extensions/XTest.h>
#include <X11/Xutil.h>

Grabber::Grabber() {
	current.grab = false;
	current.suspend = false;
	current.all = false;
	current.xi = false;
	get_button();
	wm_state = XInternAtom(dpy, "WM_STATE", False);

	xinput = init_xi();

	init(ROOT, 0);
}

// Goddammit!
extern "C" {
typedef struct _XButtonInfo2 {
	XID        klass;
	int        length;
	short        num_buttons;
} XButtonInfo2;
}

bool Grabber::init_xi() {
	xi_devs_n = 0;
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

		if (dev->use == 0 || dev->use == IsXKeyboard || dev->use == IsXPointer)
			continue;

		bool has_button = false;
		for (int j = 0; j < dev->num_classes; j++)
			if (((XButtonInfo2 *)(& dev->inputclassinfo[j]))->klass == ButtonClass)
				has_button = true;

		if (!has_button)
			continue;

		if (verbosity >= 1)
			printf("Opening Device %s...\n", dev->name);

		XiDevice *xi_dev = new XiDevice;
		xi_dev->dev = XOpenDevice(dpy, dev->id);
		if (!xi_dev) {
			delete xi_dev;
			continue;
		}

		DeviceButtonPress(xi_dev->dev, xi_dev->button_down, xi_dev->button_events[0]);
		DeviceButtonRelease(xi_dev->dev, xi_dev->button_up, xi_dev->button_events[1]);
		xi_dev->button_events_n = 2;

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

#define ENSURE(p) while (p) { printf("Grab failed, retrying...\n"); usleep(10000); }
void Grabber::set(State s) {
	Goal old_goal = goal(current);
	Goal new_goal = goal(s);
	current = s;
	if (old_goal == new_goal)
		return;
	if (old_goal == BUTTON) {
		for (int i = 0; i < xi_devs_n; i++)
		       	XUngrabDeviceButton(dpy, xi_devs[i]->dev, button, state, NULL, ROOT);
		XUngrabButton(dpy, button, state, ROOT);
	}
	if (old_goal == ALL)
		XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
	if (old_goal == XI)
		for (int i = 0; i < xi_devs_n; i++)
			XUngrabDeviceButton(dpy, xi_devs[i]->dev, AnyButton, AnyModifier, NULL, ROOT);
	if (new_goal == BUTTON) {
		ENSURE(!XGrabButton(dpy, button, state, ROOT, False,
					ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
					GrabModeAsync, GrabModeAsync, None, None))
		for (int i = 0; i < xi_devs_n; i++)
			ENSURE(xinput && XGrabDeviceButton(dpy, xi_devs[i]->dev, button, state,
						NULL, ROOT, False, xi_devs[i]->button_events_n, xi_devs[i]->button_events,
						GrabModeAsync, GrabModeAsync))
	}
	if (new_goal == ALL)
		ENSURE(!XGrabButton(dpy, AnyButton, AnyModifier, ROOT, False,
					ButtonPressMask, GrabModeSync, GrabModeAsync, None, None))
	if (new_goal == XI)
		for (int i = 0; i < xi_devs_n; i++)
			ENSURE(xinput && XGrabDeviceButton(dpy, xi_devs[i]->dev, AnyButton, AnyModifier, NULL, ROOT, False,
						xi_devs[i]->button_events_n, xi_devs[i]->button_events,
						GrabModeAsync, GrabModeAsync))
}
#undef ENSURE

void Grabber::get_button() {
	Ref<ButtonInfo> ref(prefs().button);
	button = ref->button;
	state = ref->state;
}

void Grabber::fake_button() {
	ButtonInfo bi = prefs().click.get();
	if (bi.special == SPECIAL_IGNORE)
		return;
	int b = (bi.special == SPECIAL_DEFAULT) ? button : bi.button;
	suspend();
	bi.press();
	XTestFakeButtonEvent(dpy, b, True, CurrentTime);
	XTestFakeButtonEvent(dpy, b, False, CurrentTime);
	bi.release();
	restore();
}

void Grabber::ignore(int b) {
	before();
	XAllowEvents(dpy, ReplayPointer, CurrentTime);
	after();
	State s = current;
	s.all = false;
	set(s);
}

void Grabber::create(Window w) {
	XClassHint ch;
	if (!XGetClassHint(dpy, w, &ch))
		return;
	wins[w] = ch.res_name;
	XFree(ch.res_name);
	XFree(ch.res_class);
	XSetWindowAttributes attr;
	attr.event_mask = EnterWindowMask;
	XChangeWindowAttributes(dpy, w, CWEventMask, &attr);
}
