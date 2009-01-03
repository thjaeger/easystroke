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
#include <boost/shared_ptr.hpp>
#include <set>
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
	bool xinput;
	bool is_event(int, EventType);

	struct XiDevice {
		std::string name;
		XDevice *dev;
		XEventClass events[6];
		int event_type[6];
		int all_events_n;
		int min_x, max_x, min_y, max_y;
		bool absolute;
		void fake_press(int b, int core);
		void fake_release(int b, int core);
	};

	XiDevice *xi_dev;

	unsigned int get_device_button_state(XiDevice *&dev);
	XiDevice *get_xi_dev(XID id);

	int nMajor;
private:
	int button_events_n;
	bool init_xi();

	State current, grabbed;
	bool xi_grabbed;
	bool xi_devs_grabbed;

	void set();
	void grab_xi(bool);
	void grab_xi_devs(bool);
public:
	Grabber();

	void fake_button(int b);
	void grab(State s) { current = s; set(); }
	bool update_device_list();
};


bool no_xi = false;
Grabber *grabber = 0;

unsigned int ignore_mods[4] = { LockMask, Mod2Mask, LockMask | Mod2Mask, 0 };

const char *Grabber::state_name[6] = { "None", "Button", "All (Sync)", "All (Async)", "Scroll", "Select" };

Grabber::Grabber() {
	current = BUTTON;
	grabbed = NONE;
	xi_grabbed = false;
	xi_devs_grabbed = false;
	xinput = init_xi();
}

float rescaleValuatorAxis(int coord, int fmin, int fmax, int tmax) {
	if (fmin >= fmax)
		return coord;
	return ((float)(coord - fmin)) * (tmax + 1) / (fmax - fmin + 1);
}

bool Grabber::init_xi() {
	button_events_n = 3;
	if (no_xi)
		return false;
	int nFEV, nFER;
	if (!XQueryExtension(dpy,INAME,&nMajor,&nFEV,&nFER))
		return false;

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
		XGrabDevice(dpy, xi_dev->dev, ROOT, False,
					xi_dev->all_events_n,
					xi_dev->events, GrabModeAsync, GrabModeAsync, CurrentTime);
	} else
		XUngrabDevice(dpy, xi_dev->dev, CurrentTime);
}

void Grabber::set() {
	grab_xi(true);
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

void Grabber::XiDevice::fake_press(int b, int core) {
	XTestFakeDeviceButtonEvent(dpy, dev, b, True,  0, 0, 0);
}
void Grabber::XiDevice::fake_release(int b, int core) {
	XTestFakeDeviceButtonEvent(dpy, dev, b, False, 0, 0, 0);
}

struct Triple {
       Time t;
};
typedef boost::shared_ptr<Triple> RTriple;

RTriple create_triple(Time t) {
       RTriple e(new Triple);
       e->t = t;
       return e;
}

int verbosity = 3;

Display *dpy;

Grabber::XiDevice *current_dev = 0;
std::set<guint> xinput_pressed;
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
	virtual void press(guint b, RTriple e) {}
	virtual void release(guint b, RTriple e) {}
	// Note: We need to make sure that this calls replay/discard otherwise
	// we could leave X in an unpleasant state.
	virtual void pressure() {}
	virtual void proximity_out() {}
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

class Remapper {
	void handle_grabs() {
		guint n = XGetPointerMapping(dpy, 0, 0);
		for (guint i = 1; i<=n; i++) {
			bool is_grabbed = !!core_grabs.count(i);
			if (is_grabbed == (map(i) != i))
				continue;
			if (!is_grabbed) {
				if (verbosity >= 2)
					printf("Grabbing button %d\n", i);
				XGrabButton(dpy, i, AnyModifier, ROOT, False, ButtonPressMask,
						GrabModeAsync, GrabModeAsync, None, None);
				core_grabs.insert(i);
			} else {
				if (verbosity >= 2)
					printf("Ungrabbing button %d\n", i);
				XUngrabButton(dpy, i, AnyModifier, ROOT);
				core_grabs.erase(i);
			}

		}
	}
protected:
	virtual guint map(guint b) = 0;
public:
	static std::set<guint> core_grabs;
	// This can potentially mess up the user's mouse, make sure that it doesn't fail
	void remap(Grabber::XiDevice *xi_dev) {
		int ret;
		do {
			int n = XGetPointerMapping(dpy, 0, 0);
			unsigned char m[n];
			for (int i = 1; i<=n; i++)
				m[i-1] = map(i);
			while (MappingBusy == (ret = XSetPointerMapping(dpy, m, n)))
				for (int i = 1; i<=n; i++)
					XTestFakeButtonEvent(dpy, i, False, CurrentTime);
		} while (ret == BadValue);
		if (!xi_dev)
			return;
		XDevice *dev = xi_dev->dev;
		do {
			int n = XGetDeviceButtonMapping(dpy, dev, 0, 0);
			unsigned char m[n];
			for (int i = 0; i<n; i++)
				m[i] = i+1;
			while (MappingBusy == (ret = XSetDeviceButtonMapping(dpy, dev, m, n)))
				for (int i = 1; i<=n; i++)
					xi_dev->fake_release(i, 0);
		} while (ret == BadValue);
	}
};

std::set<guint> Remapper::core_grabs;

void reset_buttons() {
	struct Reset : public Remapper {
		guint map(guint b) { return b; }
	} reset;
	reset.remap(0);
}

void bail_out() {
	handler->replace_child(0);
	for (int i = 1; i <= 9; i++)
		XTestFakeButtonEvent(dpy, i, False, CurrentTime);
	discard(CurrentTime);
	reset_buttons();
	XFlush(dpy);
}

int (*oldIOHandler)(Display *) = 0;

int xIOErrorHandler(Display *dpy2) {
	if (dpy != dpy2)
		return oldIOHandler(dpy2);
	printf(_("Fatal Error: Connection to X server lost, restarting...\n"));
	abort();
	return 0;
}

inline float abs(float x) { return x > 0 ? x : -x; }

class AdvancedHandler : public Handler, Remapper {
	RTriple e;
	guint remap_from, remap_to;

	guint button, button2;

public:
	AdvancedHandler( RTriple e_, guint b, guint b2) : e(e_), remap_from(0), button(b), button2(b2) {}
	guint map(guint b) {
		return b;
	}
	virtual void init() {
			replay(CurrentTime); // TODO
			if (1)
				XTestFakeRelativeMotionEvent(dpy, 0, 0, 5);
			parent->replace_child(NULL);
			return;
	}
	virtual ~AdvancedHandler() {
//		reset_buttons();
	}
	virtual std::string name() { return "Advanced"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

class StrokeHandler : public Handler {
	guint button;
	RTriple last;
	Time press_t;
protected:
	virtual void release(guint b, RTriple e) {
		parent->replace_child(new AdvancedHandler(last, button, button));
		XFlush(dpy);
	}
public:
	StrokeHandler(guint b, RTriple e) : button(b), last(e), press_t(e->t) {}
	virtual std::string name() { return "Stroke"; }
	virtual Grabber::State grab_mode() { return Grabber::BUTTON; }
};

class IdleHandler : public Handler {
protected:
	virtual void init() {
		reset_buttons();
	}
	virtual void press(guint b, RTriple e) {
		replace_child(new StrokeHandler(b, e));
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
	if (dead)
		bail_out();
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
			if (xinput_pressed.size())
				if (!current_dev || current_dev->dev->device_id != bev->deviceid)
					continue;
			current_dev = grabber->get_xi_dev(bev->deviceid);
			xinput_pressed.insert(bev->button);
			handler->top()->press(bev->button, create_triple(bev->time));
		}
		if (grabber->is_event(ev.type, Grabber::UP)) {
			XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
			if (verbosity >= 3)
				printf("Release (Xi): %d (%d, %d, %d, %d, %d)\n", bev->button, bev->x, bev->y,
						bev->axis_data[0], bev->axis_data[1], bev->axis_data[2]);
			if (!current_dev || current_dev->dev->device_id != bev->deviceid)
				continue;
			xinput_pressed.erase(bev->button);
			handler->top()->release(bev->button, create_triple(bev->time));
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
