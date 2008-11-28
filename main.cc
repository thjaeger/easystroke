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
#include "win.h"
#include "main.h"
#include "shape.h"
#include "prefs.h"
#include "actiondb.h"
#include "prefdb.h"
#include "trace.h"
#include "annotate.h"
#include "fire.h"
#include "water.h"
#include "copy.h"
#include "grabber.h"

#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xproto.h>
// From #include <X11/extensions/XIproto.h>
// which is not C++-safe
#define X_GrabDeviceButton              17

#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>

bool show_gui = false;
extern bool no_xi;
bool experimental = false;
int verbosity = 0;
int offset_x = 0;
int offset_y = 0;

Display *dpy;

std::string config_dir;
Win *win;

Window current = 0, current_app = 0;
bool ignore = false;
bool scroll = false;
guint press_button = 0;
guint replay_button = 0;
Trace *trace = 0;
bool in_proximity = false;
boost::shared_ptr<sigc::slot<void, RStroke> > stroke_action;
Time last_press_t = 0;

std::set<guint> xinput_pressed;

Trace *init_trace() {
	try {
		switch(prefs.trace.get()) {
			case TraceNone:
				return new Trivial();
			case TraceShape:
				return new Shape();
			case TraceAnnotate:
				return new Annotate();
			case TraceFire:
				return new Fire();
			case TraceWater:
				return new Water();
			default:
				return new Copy();
		}
	} catch (DBusException e) {
		printf("Error: %s\n", e.what());
		return new Trivial();
	}
}

bool handle_stroke(RStroke s, int x, int y, int trigger, int button, int button_up = 0);

void replay(Time t) {
	XAllowEvents(dpy, ReplayPointer, t);
	if (!t || t >= last_press_t)
		last_press_t = 0;
}

void discard(Time t) {
	XAllowEvents(dpy, AsyncPointer, t);
	if (!t || t >= last_press_t)
		last_press_t = 0;
}

class Handler {
protected:
	Handler *child;
protected:
	virtual void grab() {}
	virtual void resume() { grab(); }
	virtual std::string name() = 0;
public:
	Handler *parent;
	Handler() : child(0), parent(0) {}
	Handler *top() {
		if (child)
			return child->top();
		else
			return this;
	}
	virtual void motion(RTriple e) {}
	virtual void press(guint b, RTriple e) {}
	virtual void release(guint b, RTriple e) {}
	virtual void press_repeated() {}
	virtual void press_no_xi(guint b, Time t) { replay(t); }
	virtual void pressure() {}
	virtual bool need_xi() { return true; }
	virtual void proximity_out() {}
	void replace_child(Handler *c) {
		bool had_child = child;
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
			std::cout << "New event handling stack: " << stack << std::endl;
		}
		if (child)
			child->init();
		if (!child && had_child)
			resume();
	}
	virtual void init() { grab(); }
	virtual bool idle() { return false; }
	virtual ~Handler() {
		if (child)
			delete child;
	}
};


class OSD : public Gtk::Window {
public:
	OSD(Glib::ustring txt) : Gtk::Window(Gtk::WINDOW_POPUP) {
		int w,h;
		set_accept_focus(false);
		set_border_width(20);
		WIDGET(Gtk::Label, label, "<big><b>" + txt + "</b></big>");
		label.set_use_markup();
		label.modify_fg(Gtk::STATE_NORMAL, Gdk::Color("White"));
		modify_bg(Gtk::STATE_NORMAL, Gdk::Color("RoyalBlue3"));
		set_opacity(0.75);
		add(label);
		label.show();
		get_size(w,h);
		int screen = DefaultScreen(dpy);
		move(DisplayWidth(dpy, screen) - w - 50, 50);
		show();
	}
};

class IgnoreHandler : public Handler {
	OSD osd;
public:
	IgnoreHandler() : osd("Ignore") {}
	void grab() {
		grabber->grab(Grabber::ALL_SYNC);
	}
	virtual void press(guint b, RTriple e) {
		replay(e->t);
		if (!in_proximity)
			proximity_out();
	}
	virtual void proximity_out() {
		clear_mods();
		parent->replace_child(0);
	}
	virtual bool need_xi() { return false; }
	virtual std::string name() { return "Ignore"; }
};

Handler *handler = 0;
void bail_out() {
	handler->replace_child(0);
	for (int i = 1; i <= 9; i++)
		XTestFakeButtonEvent(dpy, i, False, CurrentTime);
	discard(CurrentTime);
	XFlush(dpy);
}

int (*oldHandler)(Display *, XErrorEvent *) = 0;
int (*oldIOHandler)(Display *) = 0;

int xErrorHandler(Display *dpy2, XErrorEvent *e) {
	if (dpy != dpy2)
		return oldHandler(dpy2, e);
	if (verbosity == 0 && e->error_code == BadWindow) {
		switch (e->request_code) {
			case X_ChangeWindowAttributes:
			case X_GetProperty:
			case X_QueryTree:
				return 0;
		}
	}
	if (e->request_code == X_GrabButton || 
			(grabber && grabber->xinput && e->request_code == grabber->nMajor &&
			 e->minor_code == X_GrabDeviceButton)) {
		if (!handler || handler->idle()) {
			printf("Error: A%s grab failed.  Is easystroke already running?\n",
					e->request_code == X_GrabButton ? "" : "n XInput");
		} else {
			printf("Error: A grab failed.  Resetting...\n");
			bail_out();
		}
	} else {
		char text[64];
		XGetErrorText(dpy, e->error_code, text, sizeof text);
		char msg[16];
		snprintf(msg, sizeof msg, "%d", e->request_code);
		char def[32];
		snprintf(def, sizeof def, "request_code=%d, minor_code=%d", e->request_code, e->minor_code);
		char dbtext[128];
		XGetErrorDatabaseText(dpy, "XRequest", msg,
				def, dbtext, sizeof dbtext);
		printf("XError: %s: %s\n", text, dbtext);
	}
	return 0;
}

int xIOErrorHandler(Display *dpy2) {
	if (dpy != dpy2)
		return oldIOHandler(dpy2);
	printf("Fatal Error: Connection to X server lost\n");
	exit(EXIT_FAILURE);
}

class WaitForButtonHandler : public Handler, protected Timeout {
	guint button;
	bool down;
public:
	WaitForButtonHandler(guint b, bool d) : button(b), down(d) {
		set_timeout(100);
	}
	virtual void timeout() {
		printf("Warning: WaitForButtonHandler timed out\n");
		bail_out();
	}
	virtual void press(guint b, RTriple e) {
		discard(e->t);
		if (!down)
			return;
		if (b == button)
			parent->replace_child(0);
	}
	virtual void release(guint b, RTriple e) {
		if (down)
			return;
		if (b == button)
			parent->replace_child(0);
	}
	virtual std::string name() { return "WaitForButton"; }
};

inline float abs(float x) { return x > 0 ? x : -x; }

class AbstractScrollHandler : public Handler {
	OSD osd;
	int last_x, last_y;
	Time last_t;
	float offset_x, offset_y;

protected:
	AbstractScrollHandler() : osd("Scroll"), last_t(0), offset_x(0.0), offset_y(0.0) {}
	virtual void fake_button(int b1, int n1, int b2, int n2) {
		grabber->suspend();
		for (int i = 0; i<n1; i++) {
			XTestFakeButtonEvent(dpy, b1, True, CurrentTime);
			XTestFakeButtonEvent(dpy, b1, False, CurrentTime);
		}
		for (int i = 0; i<n2; i++) {
			XTestFakeButtonEvent(dpy, b2, True, CurrentTime);
			XTestFakeButtonEvent(dpy, b2, False, CurrentTime);
		}
		grabber->resume();
	}
	static float curve(float v) {
		return v * exp(log(abs(v))/3);
	}
public:
	virtual void motion(RTriple e) {
		if (!last_t || abs(e->x-last_x) > 100 || abs(e->y-last_y) > 100) {
			last_x = e->x;
			last_y = e->y;
			last_t = e->t;
			return;
		}
		if (e->t == last_t)
			return;
		offset_x += curve(float(e->x-last_x)/float(e->t-last_t))*float(e->t-last_t)/10.0;
		offset_y += curve(float(e->y-last_y)/float(e->t-last_t))*float(e->t-last_t)/5.0;
		last_x = e->x;
		last_y = e->y;
		last_t = e->t;
		int b1 = 0, n1 = 0, b2 = 0, n2 = 0;
		if (abs(offset_x) > 1.0) {
			n1 = floor(abs(offset_x));
			if (offset_x > 0) {
				b1 = 7;
				offset_x -= n1;
			} else {
				b1 = 6;
				offset_x += n1;
			}
		}
		if (abs(offset_y) > 1.0) {
			if (abs(offset_y) < 1.0)
				return;
			n2 = floor(abs(offset_y));
			if (offset_y > 0) {
				b2 = 5;
				offset_y -= n2;
			} else {
				b2 = 4;
				offset_y += n2;
			}
		}
		if (n1 || n2)
			fake_button(b1,n1, b2,n2);
	}
};

class ScrollHandler : public AbstractScrollHandler {
protected:
	void grab() {
		grabber->grab(Grabber::ALL_ASYNC);
	}
public:
	virtual void init() {
		grabber->grab_xi_devs(true);
		grab();
	}
	virtual void motion(RTriple e) {
		if (xinput_pressed.size())
			AbstractScrollHandler::motion(e);
	}
	virtual void press(guint b, RTriple e) {
		XTestFakeButtonEvent(dpy, b, False, CurrentTime);
	}
	virtual void release(guint b, RTriple e) {
		if (!in_proximity)
			parent->replace_child(0);
	}
	virtual void proximity_out() {
		parent->replace_child(0);
	}
	virtual std::string name() { return "Scroll"; }
	virtual ~ScrollHandler() {
		clear_mods();
		grabber->grab_xi_devs(false);
	}
};

class ScrollXiHandler : public AbstractScrollHandler {
protected:
	void grab() {
		grabber->grab(Grabber::NONE);
	}
public:
	virtual void release(guint b, RTriple e) {
		Handler *p = parent;
		p->replace_child(0);
		p->release(b, e);
	}
	~ScrollXiHandler() { clear_mods(); }
	virtual std::string name() { return "ScrollXi"; }
};

class AdvancedLegacyHandler : public Handler {
	RStroke stroke;
	RTriple e;
	guint button, button2;

	void do_press(float x, float y) {
		handle_stroke(stroke, x, y, button2, button);
		ignore = false;
		scroll = false;
		replay_button = 0;
		press_button = 0;
	}
public:
	AdvancedLegacyHandler(RStroke s, RTriple e_, guint b, guint b2) : stroke(s), e(e_), button(b), button2(b2) {}

	virtual void init() {
		do_press(e->x, e->y);
	}

	virtual void press(guint b, RTriple e) {
		if (button2)
			return;
		button2 = b;
		do_press(e->x, e->y);
	}

	virtual void release(guint b, RTriple e) {
		if (b != button && b != button2)
			return;
		if (b == button)
			button = button2;
		button2 = 0;
		if (!button && !button2)
			parent->replace_child(0);
	}
	virtual ~AdvancedLegacyHandler() {
		clear_mods();
	}

	virtual std::string name() { return "AdvancedLegacy"; }
};

class ButtonXiHandler : public Handler {
	guint emulate, pressed;
public:
	ButtonXiHandler(guint emulate_, guint pressed_) : emulate(emulate_), pressed(pressed_) {}
	virtual void init() {
		grabber->grab(Grabber::NONE);
		XTestFakeButtonEvent(dpy, emulate, True, CurrentTime);
	}
	virtual void release(guint b, RTriple e) {
		if (b != pressed)
			return;
		XTestFakeButtonEvent(dpy, emulate, False, CurrentTime);
		parent->replace_child(0);
		clear_mods();
	}
	virtual std::string name() { return "ButtonXi"; }
};

class AdvancedHandler : public Handler {
	RStroke stroke;
	RTriple e;
	int emulated_button;

	guint button, button2;
public:
	AdvancedHandler(RStroke s, RTriple e_, guint b, guint b2) :
			stroke(s), e(e_), emulated_button(0), button(b), button2(b2) {
		XTestFakeButtonEvent(dpy, button, False, CurrentTime);
		XTestFakeButtonEvent(dpy, button2, False, CurrentTime);
	}
	virtual void init() {
		grabber->grab_xi_devs(true);
		handle_stroke(stroke, e->x, e->y, button2, button);
		ignore = false;
		if (scroll) {
			scroll = false;
			Handler *h = new ScrollXiHandler;
			replace_child(h);
			h->replace_child(new WaitForButtonHandler(button2, false));
			return;
		}
		if (replay_button) {
			press_button = replay_button;
			replay_button = 0;
		}
		if (!press_button) {
			grabber->grab(Grabber::ALL_ASYNC);
			return;
		}
		grabber->suspend();
		XTestFakeButtonEvent(dpy, press_button, True, CurrentTime);
		grabber->grab(Grabber::ALL_ASYNC);
		grabber->resume();
		emulated_button = press_button;
		press_button = 0;
	}
	virtual void press(guint b, RTriple e) {
		if (button2)
			return;
		button2 = b;
		XTestFakeButtonEvent(dpy, button2, False, CurrentTime);
		handle_stroke(stroke, e->x, e->y, button, button2);
		ignore = false;
		if (scroll) {
			scroll = false;
			Handler *h = new ScrollXiHandler;
			replace_child(h);
			// Why do we not need this?
//			h->replace_child(new WaitForButtonHandler(button2, false));
			return;
		}
		if (replay_button) {
			press_button = replay_button;
			replay_button = 0;
		}
		if (!press_button)
			return;
		if (emulated_button) {
			XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
			emulated_button = 0;
		}
		grabber->suspend();
		XTestFakeButtonEvent(dpy, press_button, True, CurrentTime);
		grabber->resume();
		emulated_button = press_button;
		press_button = 0;
	}
	virtual void resume() {
		grabber->grab(Grabber::ALL_ASYNC);
	}
	virtual void release(guint b, RTriple e) {
		if (b != button && b != button2)
			return;
		if (b == button)
			button = button2;
		button2 = 0;
		if (emulated_button) {
			XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
			emulated_button = 0;
		}
		if (!button && !button2)
			parent->replace_child(0);
	}
	virtual ~AdvancedHandler() {
		if (emulated_button) {
			XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
			emulated_button = 0;
		}
		clear_mods();
		grabber->grab_xi_devs(false);
	}
	virtual std::string name() { return "Advanced"; }
};


Trace::Point orig;

struct ButtonTime {
	guint b;
	Time t;
};

Bool is_xi_press(Display *dpy, XEvent *ev, XPointer arg) {
	ButtonTime *bt = (ButtonTime *)arg;
	if (!grabber->xinput)
		return false;
	if (!grabber->is_event(ev->type, Grabber::DOWN))
		return false;
	XDeviceButtonEvent* bev = (XDeviceButtonEvent *)ev;
	if (bev->button != bt->b)
		return false;
	return bev->time == bt->t;
}

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

class StrokeHandler : public Handler, public Timeout {
	guint button;
	RPreStroke cur;
	bool is_gesture;
	bool drawing;
	RTriple last;
	bool repeated;
	float min_speed;
	float speed;
	static float k;
	bool use_timeout;
	Time press_t;

	RStroke finish(guint b) {
		trace->end();
		XFlush(dpy);
		if (!is_gesture)
			cur->clear();
		if (b && prefs.advanced_ignore.get())
			cur->clear();
		return Stroke::create(*cur, button, b, false);
	}

	virtual void timeout() {
		do_timeout();
		XFlush(dpy);
	}

	void do_timeout() {
		if (verbosity >= 2)
			printf("Aborting stroke...\n");
		trace->end();
		if (!prefs.timeout_gestures.get()) {
			replay(press_t);
			parent->replace_child(0);
			XTestFakeRelativeMotionEvent(dpy, 0, 0, 5);
			return;
		}
		if (!is_gesture)
			cur->clear();
		RStroke s = Stroke::create(*cur, button, 0, true);
		if (!handle_stroke(s, last->x, last->y, button, 0, button)) {
			replay(press_t);
			parent->replace_child(0);
			XTestFakeRelativeMotionEvent(dpy, 0, 0, 5);
			return;
		}
		discard(last->t);
		if (replay_button) {
			replay_button = false;
			printf("Error: This can't happen\n");
		};
		if (ignore) {
			ignore = false;
		}
		if (scroll) {
			scroll = false;
			parent->replace_child(new ScrollXiHandler);
			return;
		}
		if (press_button && !(!repeated && xinput_pressed.count(button) && press_button == button)) {
			parent->replace_child(new ButtonXiHandler(press_button, button));
			press_button = false;
			return;
		}
		press_button = 0;
		clear_mods();
		parent->replace_child(0);
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
		long ms = log(min_speed/speed) / k;
		set_timeout(ms);
		return false;
	}
protected:
	virtual void press_repeated() {
		repeated = true;
	}
	virtual void pressure() {
		trace->end();
		replay(press_t);
		parent->replace_child(0);
	}
	virtual void motion(RTriple e) {
		if (!repeated && xinput_pressed.count(button) && !prefs.ignore_grab.get()) {
			if (verbosity >= 2)
				printf("Ignoring xi-only stroke\n");
			parent->replace_child(0);
			return;
		}
		cur->add(e);
		float dist = hypot(e->x-orig.x, e->y-orig.y);
		if (!is_gesture && dist > prefs.radius.get())
			is_gesture = true;
		if (!drawing && dist > 4) {
			drawing = true;
			bool first = true;
			for (PreStroke::iterator i = cur->begin(); i != cur->end(); i++) {
				Trace::Point p;
				p.x = (*i)->x;
				p.y = (*i)->y;
				if (first) {
					trace->start(p);
					first = false;
				} else {
					trace->draw(p);
				}
			}
		} else if (drawing) {
			Trace::Point p;
			p.x = e->x;
			p.y = e->y;
			trace->draw(p);
		}
		calc_speed(e);
	}

	virtual void press(guint b, RTriple e) {
		if (b == button)
			return;
		if (calc_speed(e))
			return;
		RStroke s = finish(b);
		if (grabber->xinput)
			discard(press_t);

		if (stroke_action) {
			handle_stroke(s, e->x, e->y, button, b);
			parent->replace_child(0);
			return;
		}

		if (grabber->xinput)
			parent->replace_child(new AdvancedHandler(s, e, b, button));
		else
			parent->replace_child(new AdvancedLegacyHandler(s, e, b, button));
	}

	virtual void release(guint b, RTriple e) {
		if (calc_speed(e))
			return;
		RStroke s = finish(0);

		// TODO
		bool good = handle_stroke(s, e->x, e->y, button, 0);
		win->show_success(good);
		if (!good)
			XBell(dpy, 0);
		if (replay_button) {
			if (grabber->xinput)
				replay(press_t);
			else
				press_button = replay_button;
			replay_button = 0;
		} else
			if (grabber->xinput)
				discard(press_t);
		if (ignore) {
			ignore = false;
			parent->replace_child(new IgnoreHandler);
			return;
		}
		if (scroll) {
			scroll = false;
			if (grabber->xinput) {
				parent->replace_child(new ScrollHandler);
				return;
			}
		}
		if (press_button && !(!repeated && xinput_pressed.count(b) && press_button == button)) {
			grabber->fake_button(press_button);
		}
		press_button = 0;
		clear_mods();
		parent->replace_child(0);
	}
public:
	StrokeHandler(guint b, RTriple e) : button(b), is_gesture(false), drawing(false), last(e),
	repeated(false), min_speed(0.001*prefs.min_speed.get()), speed(min_speed * exp(-k*prefs.init_timeout.get())),
	use_timeout(prefs.init_timeout.get() && prefs.min_speed.get()), press_t(e->t) {
		orig.x = e->x; orig.y = e->y;
		cur = PreStroke::create();
		cur->add(e);
		if (!grabber->xinput)
			discard(press_t);
	}
	~StrokeHandler() {
		trace->end();
		if (grabber->xinput)
			discard(press_t);
	}
	virtual std::string name() { return "Stroke"; }
};

float StrokeHandler::k = -0.01;

class WorkaroundHandler : public Handler, public Timeout {
public:
	WorkaroundHandler() {
		set_timeout(100);
	}
	virtual void timeout() {
		printf("Warning: WorkaroundHandler timed out\n");
		bail_out();
	}
	virtual void press(guint b, RTriple e) {
		if (b == 1)
			return;
		grabber->fake_device_release(1);
		grabber->fake_device_release(b);
		RPreStroke p = PreStroke::create();
		RStroke s = Stroke::create(*p, b, 1, false);
		parent->replace_child(new AdvancedHandler(s, e, 1, b));
	}
	virtual bool need_xi() { return false; }
	virtual std::string name() { return "Workaround"; }
};

class IdleHandler : public Handler {
protected:
	virtual void init() {
		XGrabKey(dpy, XKeysymToKeycode(dpy,XK_Escape), AnyModifier, ROOT, True, GrabModeAsync, GrabModeSync);
		grab();
	}
	virtual void press_no_xi(guint b, Time t) {
		if (b != 1 || grabber->is_grabbed(b)) {
			replay(t);
			return;
		}
		unsigned int state = grabber->get_device_button_state();
		if (state & (state-1)) {
			discard(t);
			replace_child(new WorkaroundHandler);
		} else {
			replay(t);
		}
	}
	virtual void press(guint b, RTriple e) {
		if (xinput_pressed.size() > 1)
			return;
		if (current_app)
			activate_window(current_app, e->t);
		replace_child(new StrokeHandler(b, e));
	}
	virtual void grab() {
		grabber->grab(Grabber::BUTTON);
	}
	virtual void resume() {
		grab();
	}
public:
	virtual bool idle() { return true; }
	virtual std::string name() { return "Idle"; }
	virtual ~IdleHandler() {
		XUngrabKey(dpy, XKeysymToKeycode(dpy,XK_Escape), AnyModifier, ROOT);
	}
};

boost::shared_ptr<sigc::slot<void, std::string> > window_selected;

class SelectHandler : public Handler, public Timeout {
	bool active;
	sigc::slot<void, std::string> callback;
	virtual void timeout() {
		active = true;
		grabber->grab(Grabber::SELECT);
		XFlush(dpy);
	}
	virtual void press(guint b, RTriple e) {
		discard(e->t);
		if (!active)
			return;
		window_selected.reset(new sigc::slot<void, std::string>(callback));
		parent->replace_child(0);
	}
	virtual bool need_xi() { return false; }
	public:
	SelectHandler(sigc::slot<void, std::string> callback_) : active(false), callback(callback_) {
		win->get_window().get_window()->lower();
		set_timeout(100);
	}
	virtual std::string name() { return "Select"; }
};


void run_by_name(const char *str) {
	for (ActionDB::const_iterator i = actions.begin(); i != actions.end(); i++) {
		if (i->second.name == std::string(str)) {
			i->second.action->run();
			clear_mods();
			return;
		}
	}
	printf("Warning: No action \"%s\" defined\n", str);
}

bool dead = false;

void quit(int) {
	if (handler->top()->idle() || dead)
		Gtk::Main::quit();
	else
		dead = true;
}

struct MouseEvent;

class Main {
	std::string parse_args_and_init_gtk(int argc, char **argv);
	void create_config_dir();
	char* next_event();
	void usage(char *me, bool good);
	void version();

	std::string display;
	Gtk::Main *kit;
	int event_basep;
	bool randr;
public:
	Main(int argc, char **argv);
	void run();
	MouseEvent *get_mouse_event(XEvent &ev);
	bool handle(Glib::IOCondition);
	void handle_mouse_event(MouseEvent *me1, MouseEvent *me2);
	void handle_enter_leave(XEvent &ev);
	void handle_event(XEvent &ev);
	~Main();
};

ActionDBWatcher *action_watcher = 0;

void reload_trace() {
	Trace *new_trace = init_trace();
	delete trace;
	trace = new_trace;
}

void open_uri(Gtk::LinkButton *button, const Glib::ustring& uri) {
	if (!fork()) {
		execlp("xdg-open", "xdg-open", uri.c_str(), NULL);
		exit(EXIT_FAILURE);
	}
}

// dbus-send --type=method_call --dest=org.easystroke /org/easystroke org.easystroke.send string:"foo"
void send_dbus(char *str) {
	GError *error = 0;
	DBusGConnection *bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (!bus) {
		printf("Error initializing D-BUS\n");
		exit(EXIT_FAILURE);
	}
	DBusGProxy *proxy = dbus_g_proxy_new_for_name(bus, "org.easystroke", "/org/easystroke", "org.easystroke");
	dbus_g_proxy_call_no_reply(proxy, "send", G_TYPE_STRING, str, G_TYPE_INVALID);
}

bool start_dbus();

Main::Main(int argc, char **argv) : kit(0) {
	if (0) {
		RStroke trefoil = Stroke::trefoil();
		trefoil->draw_svg("easystroke.svg");
		exit(EXIT_SUCCESS);
	}
	if (argc > 1 && !strcmp(argv[1], "send")) {
		if (argc == 2)
			usage(argv[0], false);
		gtk_init(&argc, &argv);
		send_dbus(argv[2]);
		exit(EXIT_SUCCESS);
	}

	Glib::thread_init();
	display = parse_args_and_init_gtk(argc, argv);
	create_config_dir();
	unsetenv("DESKTOP_AUTOSTART_ID");

	signal(SIGINT, &quit);
	signal(SIGCHLD, SIG_IGN);

	Gtk::LinkButton::set_uri_hook(sigc::ptr_fun(&open_uri));

	dpy = XOpenDisplay(display.c_str());
	if (!dpy) {
		printf("Couldn't open display\n");
		exit(EXIT_FAILURE);
	}

	action_watcher = new ActionDBWatcher;
	action_watcher->init();
	prefs.init();

	grabber = new Grabber;
	grabber->grab(Grabber::BUTTON);

	int error_basep;
	randr = XRRQueryExtension(dpy, &event_basep, &error_basep);
	if (randr)
		XRRSelectInput(dpy, ROOT, RRScreenChangeNotifyMask);

	trace = init_trace();
	Notifier *trace_notify = new Notifier(sigc::ptr_fun(&reload_trace));
	prefs.trace.connect(trace_notify);
	prefs.color.connect(trace_notify);

	handler = new IdleHandler;
	handler->init();
	XTestGrabControl(dpy, True);

	start_dbus();
}

Glib::Dispatcher *allower = 0;
bool needs_allowing = false;

void allow_events() {
	printf("Warning: press without corresponding release, resetting...\n");
	bail_out();
	needs_allowing = false;
}

void check_endless() {
	Time last_t;
	for (;;) {
		last_t = last_press_t;
		sleep(5);
		if (needs_allowing) {
			printf("Error: Endless loop detected\n");
			raise(SIGKILL);
		}
		if (last_t && last_t == last_press_t) {
			needs_allowing = true;
			(*allower)();
		}
	}
}

void Main::run() {
	Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(dpy), Glib::IO_IN);
	io->connect(sigc::mem_fun(*this, &Main::handle));
	io->attach();
	allower = new Glib::Dispatcher;
	allower->connect(sigc::ptr_fun(&allow_events));
	Glib::Thread::create(sigc::ptr_fun(&check_endless), false);
	win = new Win;
	if (show_gui)
		win->get_window().show();
	Gtk::Main::run();
	delete win;
}

void Main::usage(char *me, bool good) {
	printf("The full easystroke documentation is available at the following address:\n");
	printf("\n");
	printf("http://easystroke.wiki.sourceforge.net/Documentation#content\n");
	printf("\n");
	printf("Usage: %s [OPTION]...\n", me);
	printf("or:    %s send <action_name>\n", me);
	printf("\n");
	printf("Options:\n");
	printf("  -c, --config-dir       Directory for config files\n");
	printf("      --display          X Server to contact\n");
	printf("  -x  --no-xi            Don't use the Xinput extension\n");
	printf("  -e  --experimental     Start in experimental mode\n");
	printf("  -g, --show-gui         Show the configuration dialog on startup\n");
	printf("      --offset-x         XInput workaround\n");
	printf("      --offset-y         XInput workaround\n");
	printf("  -v, --verbose          Increase verbosity level\n");
	printf("  -h, --help             Display this help and exit\n");
	printf("      --version          Output version information and exit\n");
	exit(good ? EXIT_SUCCESS : EXIT_FAILURE);
}

extern const char *version_string;
void Main::version() {
	printf("easystroke %s\n", version_string);
	printf("\n");
	printf("Written by Thomas Jaeger <ThJaeger@gmail.com>.\n");
	exit(EXIT_SUCCESS);
}

std::string Main::parse_args_and_init_gtk(int argc, char **argv) {
	static struct option long_opts1[] = {
		{"display",1,0,'d'},
		{"help",0,0,'h'},
		{"version",0,0,'V'},
		{"show-gui",0,0,'g'},
		{"no-xi",1,0,'x'},
		{0,0,0,0}
	};
	static struct option long_opts2[] = {
		{"config-dir",1,0,'c'},
		{"display",1,0,'d'},
		{"experimental",0,0,'e'},
		{"show-gui",0,0,'g'},
		{"no-xi",0,0,'x'},
		{"verbose",0,0,'v'},
		{"offset-x",1,0,'X'},
		{"offset-y",1,0,'Y'},
		{0,0,0,0}
	};
	std::string display;
	char opt;
	// parse --display here, before Gtk::Main(...) takes it away from us
	opterr = 0;
	while ((opt = getopt_long(argc, argv, "ghx", long_opts1, 0)) != -1)
		switch (opt) {
			case 'd':
				display = optarg;
				break;
			case 'g':
				show_gui = true;
				break;
			case 'h':
				usage(argv[0], true);
				break;
			case 'V':
				version();
				break;
			case 'x':
				no_xi = true;
				break;
		}
	optind = 1;
	opterr = 1;
	kit = new Gtk::Main(argc, argv);
	oldHandler = XSetErrorHandler(xErrorHandler);
	oldIOHandler = XSetIOErrorHandler(xIOErrorHandler);

	while ((opt = getopt_long(argc, argv, "c:egvx", long_opts2, 0)) != -1) {
		switch (opt) {
			case 'c':
				config_dir = optarg;
				break;
			case 'e':
				experimental = true;
				break;
			case 'v':
				verbosity++;
				break;
			case 'd':
			case 'n':
			case 'g':
			case 'x':
				break;
			case 'X':
				offset_x = atoi(optarg);
				break;
			case 'Y':
				offset_y = atoi(optarg);
				break;
			default:
				usage(argv[0], false);
		}
	}
	return display;
}

void Main::create_config_dir() {
	struct stat st;
	if (config_dir == "") {
		config_dir = getenv("HOME");
		config_dir += "/.easystroke";
	}
	if (lstat(config_dir.c_str(), &st) == -1) {
		if (mkdir(config_dir.c_str(), 0777) == -1) {
			printf("Error: Couldn't create configuration directory \"%s\"\n", config_dir.c_str());
			exit(EXIT_FAILURE);
		}
	} else {
		if (!S_ISDIR(st.st_mode)) {
			printf("Error: \"%s\" is not a directory\n", config_dir.c_str());
			exit(EXIT_FAILURE);
		}
	}
	config_dir += "/";
}

bool handle_stroke(RStroke s, int x, int y, int trigger, int button, int button_up) {
	bool success = false;
	s->trigger = trigger;
	s->button = (button == trigger) ? 0 : button;
	if (verbosity >= 4)
		s->print();
	if (stroke_action) {
		(*stroke_action)(s);
		stroke_action.reset();
		success = true;
	} else {
		Ranking *ranking = actions.get_action_list(grabber->get_wm_class())->handle(s, button_up);
		success = ranking->id != &stroke_not_found && ranking->id != &stroke_is_timeout;
		ranking->x = x;
		ranking->y = y;
		if (ranking->id == &stroke_is_click)
			replay_button = trigger;
		if ((ranking->id!=&stroke_is_click && ranking->id!=&stroke_is_timeout) || prefs.show_clicks.get())
			Glib::signal_idle().connect(sigc::mem_fun(ranking, &Ranking::show));
		else
			delete ranking;
	}
	return success;
}

extern Window get_app_window(Window &w);

bool translate_coordinates(XID xid, int sx, int sy, int *axis_data, float &x, float &y) {
	sx += offset_x;
	sy += offset_y;
	Grabber::XiDevice *xi_dev = grabber->get_xi_dev(xid);
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
	if (d > 4 && d2 > 4) {
		x = sx;
		y = sy;
		return false;
	}
	if (d2 < d) {
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
		win->get_window().raise();
	}
}

class PresenceWatcher : public Timeout {
	virtual void timeout() {
		grabber->update_device_list();
		win->prefs_tab->update_device_list();
	}
} presence_watcher;

void Main::handle_event(XEvent &ev) {
	switch(ev.type) {
	case KeyPress:
		if (ev.xkey.keycode != XKeysymToKeycode(dpy, XK_Escape))
			break;
		XAllowEvents(dpy, ReplayKeyboard, CurrentTime);
		if (handler->top()->idle())
			break;
		printf("Escape pressed: Resetting...\n");
		bail_out();
		break;

	case ClientMessage:
		break;

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
		if (randr && ev.type == event_basep) {
			XRRUpdateConfiguration(&ev);
			Trace *new_trace = init_trace();
			delete trace;
			trace = new_trace;
		}
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
	enum Type { PRESS, RELEASE, MOTION };
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
	case MotionNotify:
		if (verbosity >= 3)
			printf("Motion: (%d, %d)\n", ev.xmotion.x, ev.xmotion.y);
		me = new MouseEvent;
		me->type = MouseEvent::MOTION;
		me->button = 0;
		me->xi = false;
		me->t = ev.xmotion.time;
		me->x = ev.xmotion.x;
		me->y = ev.xmotion.y;
		return me;

	case ButtonPress:
		if (verbosity >= 3)
			printf("Press: %d (%d, %d)\n", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
		last_press_t = ev.xbutton.time;
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
				printf("Press (Xi): %d (%d, %d, %d, %d, %d)\n",bev->button, bev->x, bev->y,
						bev->axis_data[0], bev->axis_data[1], bev->axis_data[2]);
			xinput_pressed.insert(bev->button);
			me = new MouseEvent;
			me->type = MouseEvent::PRESS;
			me->button = bev->button;
			me->xi = true;
			me->t = bev->time;
			translate_coordinates(bev->deviceid, bev->x, bev->y, bev->axis_data, me->x_xi, me->y_xi);
			me->z_xi = 0;
			return me;
		}
		if (grabber->is_event(ev.type, Grabber::UP)) {
			XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
			if (verbosity >= 3)
				printf("Release (Xi): %d (%d, %d, %d, %d, %d)\n", bev->button, bev->x, bev->y,
						bev->axis_data[0], bev->axis_data[1], bev->axis_data[2]);
			xinput_pressed.erase(bev->button);
			me = new MouseEvent;
			me->type = MouseEvent::RELEASE;
			me->button = bev->button;
			me->xi = true;
			me->t = bev->time;
			translate_coordinates(bev->deviceid, bev->x, bev->y, bev->axis_data, me->x_xi, me->y_xi);
			me->z_xi = 0;
			return me;
		}
		if (grabber->is_event(ev.type, Grabber::MOTION)) {
			XDeviceMotionEvent* mev = (XDeviceMotionEvent *)&ev;
			if (verbosity >= 3)
				printf("Motion (Xi): (%d, %d, %d, %d, %d)\n", mev->x, mev->y,
						mev->axis_data[0], mev->axis_data[1], mev->axis_data[2]);
			me = new MouseEvent;
			me->type = MouseEvent::MOTION;
			me->button = 0;
			me->xi = true;
			me->t = mev->time;
			translate_coordinates(mev->deviceid, mev->x, mev->y, mev->axis_data, me->x_xi, me->y_xi);
			me->z_xi = 0;
			Grabber::XiDevice *xi_dev = grabber->get_xi_dev(mev->deviceid);
			if (xi_dev && xi_dev->supports_pressure)
				me->z_xi = xi_dev->normalize_pressure(mev->axis_data[2]);
			return me;
		}
		return 0;
	}
}

// Preconditions: me1 != 0
void Main::handle_mouse_event(MouseEvent *me1, MouseEvent *me2) {
	MouseEvent me;
	bool xi_1 = me1 && me1->xi, xi_2 = me2 && me2->xi;
	if (!xi_1 && !xi_2) {
		if (grabber->xinput && handler->top()->need_xi()) {
			if (me1->type == MouseEvent::PRESS)
				handler->top()->press_no_xi(me1->button, me1->t);
			delete me1;
			delete me2;
			return;
		} else {
			me = *me1;
			delete me1;
			delete me2;
			me.x_xi = me.x;
			me.y_xi = me.y;
		}
	} else {
		if (xi_1)
			me = *me1;
		else
			me = *me2;
		delete me1;
		delete me2;
	}

	switch (me.type) {
	case MouseEvent::MOTION:
		if (prefs.pressure_abort.get() && me.z_xi >= prefs.pressure_threshold.get())
			handler->top()->pressure();
		handler->top()->motion(create_triple(me.x_xi, me.y_xi, me.t));
		break;
	case MouseEvent::PRESS:
		handler->top()->press(me.button, create_triple(me.x_xi, me.y_xi, me.t));
		if (me1 && me2)
			handler->top()->press_repeated();
		break;
	case MouseEvent::RELEASE:
		handler->top()->release(me.button, create_triple(me.x_xi, me.y_xi, me.t));
		break;
	}
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
			printf("Error: A grab failed.  Resetting...\n");
			me = 0;
			bail_out();
		}
	}
	if (handler->top()->idle() && dead)
		Gtk::Main::quit();
	return true;
}

Main::~Main() {
	trace->end();
	delete grabber;
	delete trace;
	delete kit;
	XCloseDisplay(dpy);
	prefs.execute_now();
	action_watcher->execute_now();
}

int main(int argc, char **argv) {
	Main mn(argc, argv);
	mn.run();
	if (verbosity >= 2)
		printf("Exiting...\n");
	return EXIT_SUCCESS;
}

bool SendKey::run() {
	press();
	XTestFakeKeyEvent(dpy, code, true, 0);
	XTestFakeKeyEvent(dpy, code, false, 0);
	return true;
}

bool Button::run() {
		press();
		press_button = button;
		return true;
}

bool Scroll::run() {
	press();
	scroll = true;
	return true;
}

struct does_that_really_make_you_happy_stupid_compiler {
	guint mask;
	guint sym;
} modkeys[] = {
	{GDK_SHIFT_MASK, XK_Shift_L},
	{GDK_CONTROL_MASK, XK_Control_L},
	{GDK_MOD1_MASK, XK_Alt_L},
	{GDK_MOD2_MASK, 0},
	{GDK_MOD3_MASK, 0},
	{GDK_MOD4_MASK, 0},
	{GDK_MOD5_MASK, 0},
	{GDK_SUPER_MASK, XK_Super_L},
	{GDK_HYPER_MASK, XK_Hyper_L},
	{GDK_META_MASK, XK_Meta_L},
};
int n_modkeys = 10;

guint mod_state = 0;

void set_mod_state(int new_state) {
	for (int i = 0; i < n_modkeys; i++) {
		guint mask = modkeys[i].mask;
		if ((mod_state & mask) ^ (new_state & mask))
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, modkeys[i].sym), new_state & mask, 0);
	}
	mod_state = new_state;
}

void ButtonInfo::press() {
	set_mod_state(state);
}

void ModAction::press() {
	set_mod_state(mods);
}

void clear_mods() {
	set_mod_state(0);
}

bool Ignore::run() {
	press();
	ignore = true;
	return true;
}

bool Misc::run() {
	switch (type) {
		case SHOWHIDE:
			win->show_hide();
			return true;
		case UNMINIMIZE:
			grabber->unminimize();
			return true;
		case DISABLE:
			win->toggle_disabled();
		default:
			return true;
	}
}
