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
#include "main.h"
#include "grabber.h"
#include "util.h"

#include <glibmm/i18n.h>

#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/Xproto.h>
// From #include <X11/extensions/XIproto.h>
// which is not C++-safe
#define X_GrabDeviceButton              17

#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>

#include <set>

struct Triple {
       float x;
       float y;
       Time t;
};
typedef boost::shared_ptr<Triple> RTriple;

RTriple create_triple(float x, float y, Time t) {
       RTriple e(new Triple);
       e->x = x;
       e->y = y;
       e->t = t;
       return e;
}

bool show_gui = false;
extern bool no_xi;
bool rotated = false;
bool experimental = false;
int verbosity = 0;
int offset_x = 0;
int offset_y = 0;

Display *dpy;
int argc;
char **argv;

std::string config_dir;

Window current = 0, current_app = 0;
bool in_proximity = false;
Grabber::XiDevice *current_dev = 0;
std::set<guint> xinput_pressed;
bool dead = false;

class Handler;
Handler *handler = 0;
boost::shared_ptr<sigc::slot<void, std::string> > window_selected;

void replay(Time t) { XAllowEvents(dpy, ReplayPointer, t); }
void discard(Time t) { XAllowEvents(dpy, AsyncPointer, t); }

struct Point {
	int x, y;
};

class Timeout {
	sigc::connection *c;
	boost::shared_ptr<sigc::connection> copy;
	bool to() { timeout(); c = 0; return false; }
public:
	Timeout() : c(0) {}
protected:
	virtual void timeout() = 0;
public:
	bool remove_timeout() {
		if (c) {
			c->disconnect();
			c = 0;
			return true;
		}
		return false;
	}
	void set_timeout(int ms) {
		remove_timeout();
		c = new sigc::connection(Glib::signal_timeout().connect(sigc::mem_fun(*this, &Timeout::to), ms));
		copy.reset(c);
	}
	virtual ~Timeout() {
		remove_timeout();
	}
};

Point orig;

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
	virtual void press_core(guint b, Time t, bool xi) { replay(t); }
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
		reset_buttons();
	}
	virtual std::string name() { return "Advanced"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

XAtom ATOM("ATOM");

Atom get_atom(Window w, Atom prop) {
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop_return = NULL;

	if (XGetWindowProperty(dpy, w, prop, 0, sizeof(Atom), False, *ATOM, &actual_type, &actual_format,
				&nitems, &bytes_after, &prop_return) != Success)
		return None;
	if (!prop_return)
		return None;
	Atom atom = *(Atom *)prop_return;
	XFree(prop_return);
	return atom;
}

bool has_atom(Window w, Atom prop, Atom value) {
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop_return = NULL;

	if (XGetWindowProperty(dpy, w, prop, 0, sizeof(Atom), False, *ATOM, &actual_type, &actual_format,
				&nitems, &bytes_after, &prop_return) != Success)
		return None;
	if (!prop_return)
		return None;
	Atom *atoms = (Atom *)prop_return;
	bool ans = false;
	for (unsigned long i = 0; i < nitems; i++)
		if (atoms[i] == value)
			ans = true;
	XFree(prop_return);
	return ans;
}

Window get_window(Window w, Atom prop) {
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop_return = NULL;
	static XAtom WINDOW("WINDOW");

	if (XGetWindowProperty(dpy, w, prop, 0, sizeof(Atom), False, *WINDOW, &actual_type, &actual_format,
				&nitems, &bytes_after, &prop_return) != Success)
		return None;
	if (!prop_return)
		return None;
	Window ret = *(Window *)prop_return;
	XFree(prop_return);
	return ret;
}

void icccm_client_message(Window w, Atom a, Time t) {
	static XAtom WM_PROTOCOLS("WM_PROTOCOLS");
	XClientMessageEvent ev;
	ev.type = ClientMessage;
	ev.window = w;
	ev.message_type = *WM_PROTOCOLS;
	ev.format = 32;
	ev.data.l[0] = a;
	ev.data.l[1] = t;
	XSendEvent(dpy, w, False, 0, (XEvent *)&ev);
}

void activate_window(Window w, Time t) {
	static XAtom _NET_WM_WINDOW_TYPE("_NET_WM_WINDOW_TYPE");
	static XAtom _NET_WM_WINDOW_TYPE_DOCK("_NET_WM_WINDOW_TYPE_DOCK");
	static XAtom WM_PROTOCOLS("WM_PROTOCOLS");
	static XAtom WM_TAKE_FOCUS("WM_TAKE_FOCUS");

	Atom window_type = get_atom(w, *_NET_WM_WINDOW_TYPE);
	if (window_type == *_NET_WM_WINDOW_TYPE_DOCK)
		return;
	XWMHints *wm_hints = XGetWMHints(dpy, w);
	if (wm_hints) {
		bool input = wm_hints->input;
		XFree(wm_hints);
		if (!input)
			return;
	}
	if (verbosity >= 3)
		printf("Giving focus to window 0x%lx\n", w);

	bool take_focus = has_atom(w, *WM_PROTOCOLS, *WM_TAKE_FOCUS);
	if (take_focus)
		icccm_client_message(w, *WM_TAKE_FOCUS, t);
	else
		XSetInputFocus(dpy, w, RevertToParent, t);
}

// TODO: Check discard/replay
class StrokeHandler : public Handler, public Timeout {
	guint button;
	bool is_gesture;
	bool drawing;
	RTriple last;
	bool repeated;
	float min_speed;
	float speed;
	static float k;
	bool use_timeout;
	Time press_t;

	virtual void timeout() {
		printf("timeout()\n");
		do_timeout();
		XFlush(dpy);
	}

	void do_timeout() {
		printf("timeout\n");
		parent->replace_child(new AdvancedHandler(last, button, button));
	}

	bool calc_speed(RTriple e) {
		if (!grabber->xinput || !use_timeout)
			return false;
		int dt = e->t - last->t;
		float c = exp(k * dt);
		if (dt) {
			float dist = hypot(e->x-last->x, e->y-last->y);
			speed = c * speed + (1-c) * dist/dt;
		} else {
			speed = c * speed;
		}
		last = e;

		if (speed < min_speed) {
			timeout();
			return true;
		}
		long ms = (long)(log(min_speed/speed) / k);
		set_timeout(ms);
		return false;
	}
protected:
	virtual void press_core(guint b, Time t, bool xi) {
		if (!xi)
			discard(t);
		// At this point we already have an xi press, so we are
		// guarenteed to either get another press or a release.
		repeated = true;
	}
	virtual void pressure() {
		replay(press_t);
		printf("pressure!!\n");
		parent->replace_child(0);
	}
	virtual void press(guint b, RTriple e) {
		if (b == button)
			return;
		if (calc_speed(e))
			return;

		printf("advanced\n");
		parent->replace_child(new AdvancedHandler(e, button, b));
	}

	virtual void release(guint b, RTriple e) {
		if (calc_speed(e))
			return;
		if (grabber->xinput)
			replay(press_t);
		printf("no timeout!!\n");
		parent->replace_child(0);
	}
public:
	StrokeHandler(guint b, RTriple e) : button(b), is_gesture(false), drawing(false), last(e),
	repeated(false), min_speed(0.001*500), speed(min_speed * exp(-k*20)),
	use_timeout(true), press_t(e->t) {
		orig.x = e->x; orig.y = e->y;
		if (!grabber->xinput)
			discard(press_t);
	}
	~StrokeHandler() {
	}
	virtual std::string name() { return "Stroke"; }
	virtual Grabber::State grab_mode() { return Grabber::BUTTON; }
};

float StrokeHandler::k = -0.01;

class IdleHandler : public Handler, Timeout {
protected:
	virtual void init() {
		reset_buttons();
	}
	virtual void press_core(guint b, Time t, bool xi) {
		if (xi)
			return;
		replay(t);
	}
	virtual void timeout() { discard(CurrentTime); }
	virtual void press(guint b, RTriple e) {
		if (grabber->is_instant(b)) {
			remove_timeout();
			replace_child(new AdvancedHandler(e, b, b));
			return;
		}
		if (current_app)
			activate_window(current_app, e->t);
		remove_timeout();
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

class SelectHandler : public Handler, public Timeout {
	bool active;
	sigc::slot<void, std::string> callback;
	virtual void timeout() {
		active = true;
		grabber->grab(Grabber::SELECT);
		XFlush(dpy);
	}
	virtual void press_core(guint b, Time t, bool xi) {
		discard(t);
		if (!active)
			return;
		window_selected.reset(new sigc::slot<void, std::string>(callback));
		parent->replace_child(0);
	}
public:
	SelectHandler(sigc::slot<void, std::string> callback_) : active(false), callback(callback_) {
		set_timeout(100);
	}
	virtual std::string name() { return "Select"; }
	virtual Grabber::State grab_mode() { return Grabber::ALL_SYNC; }
};

void quit(int) {
	if (dead)
		bail_out();
	if (handler->top()->idle() || dead)
		Gtk::Main::quit();
	else
		dead = true;
}

struct MouseEvent;

class Main {
	char* next_event();

	std::string display;
	Gtk::Main *kit;
public:
	Main();
	void run();
	MouseEvent *get_mouse_event(XEvent &ev);
	bool handle(Glib::IOCondition);
	void handle_mouse_event(MouseEvent *me1, MouseEvent *me2);
	void handle_enter_leave(XEvent &ev);
	void handle_event(XEvent &ev);
	~Main();
};

Main::Main() : kit(0) {
	kit = new Gtk::Main(argc, argv);
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

	Glib::RefPtr<Gdk::Screen> screen = Gdk::Display::get_default()->get_default_screen();

	handler = new IdleHandler;
	handler->init();
	XTestGrabControl(dpy, True);

}

void Main::run() {
	Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(dpy), Glib::IO_IN);
	io->connect(sigc::mem_fun(*this, &Main::handle));
	io->attach();
	// TODO
	Gtk::Main::run();
}

extern Window get_app_window(Window &w);

int current_x, current_y;

void translate_coords(XID xid, int *axis_data, float &x, float &y) {
	Grabber::XiDevice *xi_dev = grabber->get_xi_dev(xid);
	if (!xi_dev->absolute) {
		current_x += axis_data[0];
		current_y += axis_data[1];
		x = current_x;
		y = current_y;
		return;
	}
	int w = DisplayWidth(dpy, DefaultScreen(dpy)) - 1;
	int h = DisplayHeight(dpy, DefaultScreen(dpy)) - 1;
	if (!rotated) {
		x = rescaleValuatorAxis(axis_data[0], xi_dev->min_x, xi_dev->max_x, w);
		y = rescaleValuatorAxis(axis_data[1], xi_dev->min_y, xi_dev->max_y, h);
	} else {
		x = rescaleValuatorAxis(axis_data[0], xi_dev->min_y, xi_dev->max_y, w);
		y = rescaleValuatorAxis(axis_data[1], xi_dev->min_x, xi_dev->max_x, h);
	}
}

bool translate_known_coords(XID xid, int sx, int sy, int *axis_data, float &x, float &y) {
	sx += offset_x;
	sy += offset_y;
	Grabber::XiDevice *xi_dev = grabber->get_xi_dev(xid);
	if (!xi_dev->absolute) {
		current_x = sx;
		current_y = sy;
		x = current_x;
		y = current_y;
		return true;
	}
	int w = DisplayWidth(dpy, DefaultScreen(dpy)) - 1;
	int h = DisplayHeight(dpy, DefaultScreen(dpy)) - 1;
	x        = rescaleValuatorAxis(axis_data[0], xi_dev->min_x, xi_dev->max_x, w);
	y        = rescaleValuatorAxis(axis_data[1], xi_dev->min_y, xi_dev->max_y, h);
	if (axis_data[0] == sx && axis_data[1] == sy)
		return true;
	float x2 = rescaleValuatorAxis(axis_data[0], xi_dev->min_y, xi_dev->max_y, w);
	float y2 = rescaleValuatorAxis(axis_data[1], xi_dev->min_x, xi_dev->max_x, h);
	float d  = hypot(x - sx, y - sy);
	float d2 = hypot(x2 - sx, y2 - sy);
	if (d > 2 && d2 > 2) {
		x = sx;
		y = sy;
		return false;
	}
	if (d > 2)
		rotated = true;
	if (d2 > 2)
		rotated = false;
	if (rotated) {
		x = x2;
		y = y2;
	}
	return true;
}

void Main::handle_enter_leave(XEvent &ev) {
	do {
		if (ev.xcrossing.mode == NotifyGrab)
			continue;
		if (ev.xcrossing.detail == NotifyInferior)
			continue;
		if (ev.type == EnterNotify) {
			current = ev.xcrossing.window;
			current_app = get_app_window(current);
			if (verbosity >= 3)
				printf("Entered window 0x%lx -> 0x%lx\n", ev.xcrossing.window, current_app);
		} else if (ev.type == LeaveNotify) {
			if (ev.xcrossing.window != current)
				continue;
			if (verbosity >= 3)
				printf("Left window 0x%lx\n", ev.xcrossing.window);
			current = 0;
		} else printf("Error: Bogus Enter/Leave event\n");
	} while (window_selected && XCheckMaskEvent(dpy, EnterWindowMask|LeaveWindowMask, &ev));
	grabber->update(current);
	if (window_selected) {
		(*window_selected)(grabber->get_wm_class());
		window_selected.reset();
	}
}

class PresenceWatcher : public Timeout {
	virtual void timeout() {
		grabber->update_device_list();
	}
} presence_watcher;

void Main::handle_event(XEvent &ev) {
	switch(ev.type) {
	case EnterNotify:
	case LeaveNotify:
		handle_enter_leave(ev);
		break;
	case PropertyNotify:
		static XAtom WM_CLASS("WM_CLASS");
		if (current && ev.xproperty.window == current && ev.xproperty.atom == *WM_CLASS)
			grabber->update(current);
		break;

	default:
		if (grabber->proximity_selected) {
			if (grabber->is_event(ev.type, Grabber::PROX_IN)) {
				in_proximity = true;
				if (verbosity >= 3)
					printf("Proximity: In\n");
			}
			if (grabber->is_event(ev.type, Grabber::PROX_OUT)) {
				in_proximity = false;
				if (verbosity >= 3)
					printf("Proximity: Out\n");
				handler->top()->proximity_out();
			}
		}
		if (ev.type == grabber->event_presence) {
			if (verbosity >= 2)
				printf("Device Presence\n");
			presence_watcher.set_timeout(2000);
		}
	}
}

void update_current() {
	grabber->update(current);
}

void suspend_flush() {
	grabber->suspend();
	XFlush(dpy);
}

void resume_flush() {
	grabber->resume();
	XFlush(dpy);
}

void select_window(sigc::slot<void, std::string> f) {
	handler->top()->replace_child(new SelectHandler(f));
}

struct MouseEvent {
	enum Type { PRESS, RELEASE };
	Type type;
	guint button;
	bool xi;
	int x, y;
	Time t;
	float x_xi, y_xi, z_xi;
};

MouseEvent *Main::get_mouse_event(XEvent &ev) {
	MouseEvent *me = 0;
	switch(ev.type) {
	case ButtonPress:
		if (verbosity >= 3)
			printf("Press: %d (%d, %d) at t = %ld\n", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y, ev.xbutton.time);
		me = new MouseEvent;
		me->type = MouseEvent::PRESS;
		me->button = ev.xbutton.button;
		me->xi = false;
		me->t = ev.xbutton.time;
		me->x = ev.xbutton.x;
		me->y = ev.xbutton.y;
		return me;

	case ButtonRelease:
		if (verbosity >= 3)
			printf("Release: %d (%d, %d)\n", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
		me = new MouseEvent;
		me->type = MouseEvent::RELEASE;
		me->button = ev.xbutton.button;
		me->xi = false;
		me->t = ev.xbutton.time;
		me->x = ev.xbutton.x;
		me->y = ev.xbutton.y;
		return me;

	default:
		if (grabber->is_event(ev.type, Grabber::DOWN)) {
			XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
			if (verbosity >= 3)
				printf("Press (Xi): %d (%d, %d, %d, %d, %d) at t = %ld\n",bev->button, bev->x, bev->y,
						bev->axis_data[0], bev->axis_data[1], bev->axis_data[2], bev->time);
			if (xinput_pressed.size())
				if (!current_dev || current_dev->dev->device_id != bev->deviceid)
					return 0;
			current_dev = grabber->get_xi_dev(bev->deviceid);
			xinput_pressed.insert(bev->button);
			me = new MouseEvent;
			me->type = MouseEvent::PRESS;
			me->button = bev->button;
			me->xi = true;
			me->t = bev->time;
			translate_known_coords(bev->deviceid, bev->x, bev->y, bev->axis_data, me->x_xi, me->y_xi);
			me->z_xi = 0;
			return me;
		}
		if (grabber->is_event(ev.type, Grabber::UP)) {
			XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
			if (verbosity >= 3)
				printf("Release (Xi): %d (%d, %d, %d, %d, %d)\n", bev->button, bev->x, bev->y,
						bev->axis_data[0], bev->axis_data[1], bev->axis_data[2]);
			if (!current_dev || current_dev->dev->device_id != bev->deviceid)
				return 0;
			xinput_pressed.erase(bev->button);
			me = new MouseEvent;
			me->type = MouseEvent::RELEASE;
			me->button = bev->button;
			me->xi = true;
			me->t = bev->time;
			translate_coords(bev->deviceid, bev->axis_data, me->x_xi, me->y_xi);
			me->z_xi = 0;
			return me;
		}
		return 0;
	}
}

// Preconditions: me1 != 0
void Main::handle_mouse_event(MouseEvent *me1, MouseEvent *me2) {
	MouseEvent me;
	bool xi = me1->xi || (me2 && me2->xi);
	bool core = !me1->xi || (me2 && !me2->xi);
	if (xi) {
		if (me1->xi)
			me = *me1;
		else
			me = *me2;
	} else {
		me = *me1;
		me.x_xi = me.x;
		me.y_xi = me.y;
	}
	delete me1;
	delete me2;

	if (!grabber->xinput || xi)
		switch (me.type) {
			case MouseEvent::PRESS:
				handler->top()->press(me.button, create_triple(me.x_xi, me.y_xi, me.t));
				break;
			case MouseEvent::RELEASE:
				handler->top()->release(me.button, create_triple(me.x_xi, me.y_xi, me.t));
				break;
		}
	if (core && me.type == MouseEvent::PRESS)
		handler->top()->press_core(me.button, me.t, xi);
}

bool Main::handle(Glib::IOCondition) {
	MouseEvent *me = 0;
	while (XPending(dpy)) {
		try {
			XEvent ev;
			XNextEvent(dpy, &ev);
			MouseEvent *me2 = get_mouse_event(ev);
			if (me && me2 && me->type == me2->type && me->button == me2->button && me->t == me2->t) {
				handle_mouse_event(me, me2);
				me = 0;
				continue;
			}
			if (me)
				handle_mouse_event(me, 0);
			me = me2;
			if (me) {
				if (!grabber->xinput || !XPending(dpy)) {
					handle_mouse_event(me, 0);
					me = 0;
				}
			} else {
				if (!grabber->handle(ev))
					handle_event(ev);
			}
		} catch (GrabFailedException) {
			printf(_("Error: A grab failed.  Resetting...\n"));
			me = 0;
			bail_out();
		}
	}
	if (handler->top()->idle() && dead)
		Gtk::Main::quit();
	return true;
}

Main::~Main() {
	delete grabber;
	delete kit;
	XCloseDisplay(dpy);
}

int main(int argc_, char **argv_) {
	argc = argc_;
	argv = argv_;
	Main mn;
	mn.run();
	if (verbosity >= 2)
		printf("Exiting...\n");
	return EXIT_SUCCESS;
}
