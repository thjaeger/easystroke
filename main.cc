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
#include <gtkmm.h>
#include <glibmm/i18n.h>
#include <signal.h>
#include <string>
#include <string.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XTest.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>


extern int verbosity;

extern Display *dpy;
#define ROOT (DefaultRootWindow(dpy))


class Grabber {
public:
	enum State { NONE, BUTTON };
	static const char *state_name[6];
	enum EventType { DOWN = 0, UP = 1, MOTION = 2, BUTTON_MOTION = 3, PROX_IN = 4, PROX_OUT = 5 };
	bool is_event(int, EventType);

	XDevice *dev;
	XEventClass events[6];
	int event_type[6];
	int all_events_n;
	int button_events_n;

	State current, grabbed;
	bool xi_devs_grabbed;

	void set();
	void grab_xi_devs(bool);
	Grabber();

	void grab(State s) { current = s; set(); }
};

Grabber *grabber = 0;

const char *Grabber::state_name[6] = { "None", "Button", "All (Sync)", "All (Async)", "Scroll", "Select" };

Grabber::Grabber() {
	current = BUTTON;
	grabbed = NONE;
	xi_devs_grabbed = false;
	button_events_n = 3;
	int n;
	XDeviceInfo *devs = XListInputDevices(dpy, &n);
	if (!devs)
		exit(EXIT_FAILURE);

	for (int i = 0; i < n; i++) {
		XDeviceInfo *dev_info = devs + i;

		if (strcmp(dev_info->name, "stylus"))
			continue;

		dev = XOpenDevice(dpy, dev_info->id);
		break;
	}
	XFreeDeviceList(devs);
	if (!dev)
		exit(EXIT_FAILURE);
	DeviceButtonPress(dev, event_type[DOWN], events[DOWN]);
	DeviceButtonRelease(dev, event_type[UP], events[UP]);
	DeviceButtonMotion(dev, event_type[BUTTON_MOTION], events[BUTTON_MOTION]);
	DeviceMotionNotify(dev, event_type[MOTION], events[MOTION]);

	all_events_n = 4;
	XGrabDeviceButton(dpy, dev, 1, 0, NULL, ROOT, False, button_events_n, events, GrabModeAsync, GrabModeAsync);
	set();
}

bool Grabber::is_event(int type, EventType et) {
	if (type == event_type[et])
		return true;
	return false;
}

void Grabber::grab_xi_devs(bool grab) {
	if (!xi_devs_grabbed == !grab)
		return;
	xi_devs_grabbed = grab;
	if (grab)
		XGrabDevice(dpy, dev, ROOT, False, all_events_n, events, GrabModeAsync, GrabModeAsync, CurrentTime);
	else
		XUngrabDevice(dpy, dev, CurrentTime);
}

void Grabber::set() {
	grab_xi_devs(current == NONE);
	State old = grabbed;
	grabbed = current;
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

int verbosity = 3;

Display *dpy;

bool dead = false;

class Handler;
Handler *handler = 0;

void replay(Time t) { XAllowEvents(dpy, ReplayPointer, t); }
void discard(Time t) { XAllowEvents(dpy, AsyncPointer, t); }

class Handler {
protected:
	Handler *child;
protected:
public:
	Handler *parent;
	Handler() : child(0), parent(0) {}
	Handler *top() {
		if (child)
			return child->top();
		else
			return this;
	}
	virtual void press() {}
	virtual void release() {}
	// Note: We need to make sure that this calls replay/discard otherwise
	// we could leave X in an unpleasant state.
	void replace_child(Handler *c) {
		if (child)
			delete child;
		child = c;
		if (child)
			child->parent = this;
		if (verbosity >= 2) {
			std::string stack;
			for (Handler *h = child ? child : this; h; h=h->parent) {
				stack = h->name() + " " + stack;
			}
			printf("New event handling stack: %s\n", stack.c_str());
		}
		Handler *new_handler = child ? child : this;
		grabber->grab(new_handler->grab_mode());
		if (child)
			child->init();
	}
	virtual void init() {}
	virtual bool idle() { return false; }
	virtual ~Handler() {
		if (child)
			delete child;
	}
	virtual std::string name() = 0;
	virtual Grabber::State grab_mode() = 0;
};

int (*oldIOHandler)(Display *) = 0;

int xIOErrorHandler(Display *dpy2) {
	if (dpy != dpy2)
		return oldIOHandler(dpy2);
	printf(_("Fatal Error: Connection to X server lost, restarting...\n"));
	abort();
	return 0;
}

class AdvancedHandler : public Handler {
public:
	virtual void init() {
		replay(CurrentTime);
		XTestFakeRelativeMotionEvent(dpy, 0, 0, 5);
		parent->replace_child(NULL);
	}
	virtual std::string name() { return "Advanced"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

class StrokeHandler : public Handler {
protected:
	virtual void release() {
		parent->replace_child(new AdvancedHandler);
		XFlush(dpy);
	}
public:
	virtual std::string name() { return "Stroke"; }
	virtual Grabber::State grab_mode() { return Grabber::BUTTON; }
};

class IdleHandler : public Handler {
protected:
	virtual void init() {
		XFlush(dpy); // WTF?
	}
	virtual void press() {
		replace_child(new StrokeHandler);
	}
public:
	virtual ~IdleHandler() {
		XUngrabKey(dpy, XKeysymToKeycode(dpy,XK_Escape), AnyModifier, ROOT);
	}
	virtual bool idle() { return true; }
	virtual std::string name() { return "Idle"; }
	virtual Grabber::State grab_mode() { return Grabber::BUTTON; }
};

void quit(int) {
	if (handler->top()->idle() || dead)
		Gtk::Main::quit();
	else
		dead = true;
}

bool handle(Glib::IOCondition) {
	while (XPending(dpy)) {
		XEvent ev;
		XNextEvent(dpy, &ev);
		if (grabber->is_event(ev.type, Grabber::DOWN)) {
			XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
			if (verbosity >= 3)
				printf("Press (Xi): %d (%d, %d, %d, %d, %d) at t = %ld\n",bev->button, bev->x, bev->y,
						bev->axis_data[0], bev->axis_data[1], bev->axis_data[2], bev->time);
			handler->top()->press();
		}
		if (grabber->is_event(ev.type, Grabber::UP)) {
			XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
			if (verbosity >= 3)
				printf("Release (Xi): %d (%d, %d, %d, %d, %d)\n", bev->button, bev->x, bev->y,
						bev->axis_data[0], bev->axis_data[1], bev->axis_data[2]);
			handler->top()->release();
		}
	}
	if (handler->top()->idle() && dead)
		Gtk::Main::quit();
	return true;
}

int main(int argc, char **argv) {
	Gtk::Main kit(argc, argv);
	oldIOHandler = XSetIOErrorHandler(xIOErrorHandler);

	signal(SIGINT, &quit);
	signal(SIGCHLD, SIG_IGN);

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		printf(_("Couldn't open display.\n"));
		exit(EXIT_FAILURE);
	}

	grabber = new Grabber;
	grabber->grab(Grabber::BUTTON);

	handler = new IdleHandler;
	handler->init();
	Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(dpy), Glib::IO_IN);
	io->connect(sigc::ptr_fun(&handle));
	io->attach();
	Gtk::Main::run();
	delete grabber;
	XCloseDisplay(dpy);
	if (verbosity >= 2)
		printf("Exiting...\n");
	return EXIT_SUCCESS;
}
