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
#include "composite.h"
#include "grabber.h"

#include <glibmm/i18n.h>

#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xproto.h>
// From #include <X11/extensions/XIproto.h>
// which is not C++-safe
#define X_GrabDeviceButton              17

#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>

extern Source<bool> disabled;

bool experimental = false;
int verbosity = 0;
const char *prefs_versions[] = { "-0.5.5", "-0.4.1", "-0.4.0", "", NULL };
const char *actions_versions[] = { "-0.4.1", "-0.4.0", "", NULL };
Source<Window> current_app_window(None);
std::string config_dir;
Win *win = NULL;
Display *dpy;
Window ROOT;
bool in_proximity = false;
boost::shared_ptr<sigc::slot<void, RStroke> > stroke_action;
Grabber::XiDevice *current_dev = 0;
std::set<guint> xinput_pressed; // TODO get rid of

static bool show_gui = false;
static bool no_dbus = false;

static int argc;
static char **argv;

static Window ping_window = 0;
static boost::shared_ptr<Trace> trace;
static std::map<guint, guint> core_inv_map;

class Handler;
static Handler *handler = 0;
static ActionDBWatcher *action_watcher = 0;

static int (*oldHandler)(Display *, XErrorEvent *) = 0;
static int (*oldIOHandler)(Display *) = 0;

static Trace *trace_composite() {
	try {
		return new Composite();
	} catch (std::exception &e) {
		if (verbosity >= 1)
			printf("Falling back to Shape method: %s\n", e.what());
		return new Shape();
	}
}

static Trace *init_trace() {
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
				return trace_composite();
		}
	} catch (DBusException &e) {
		printf(_("Error: %s\n"), e.what());
		return trace_composite();
	}

}

void fake_core_button(guint b, bool press) {
	if (core_inv_map.count(b))
		b = core_inv_map[b];
	XTestFakeButtonEvent(dpy, b, press, CurrentTime);
}

static void fake_click(guint b) {
	fake_core_button(b, true);
	fake_core_button(b, false);
}

static std::list<sigc::slot<void> > queued;

class Handler {
protected:
	Handler *child;
public:
	Handler *parent;
	Handler() : child(0), parent(0) {}
	Handler *top() {
		if (child)
			return child->top();
		else
			return this;
	}
	static bool idle() { return !handler->child; }

	virtual void motion(RTriple e) {}
	virtual void raw_motion(RTriple e, bool, bool) {}
	virtual void press(guint b, RTriple e) {}
	virtual void release(guint b, RTriple e) {}
	virtual void press_master(guint b, Time t) {}
	virtual void pressure() {}
	virtual void proximity_out() {}
	virtual void pong() {}
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
		while (queued.size() && Handler::idle()) {
			(*queued.begin())();
			queued.pop_front();
		}
	}
	virtual void init() {}
	virtual ~Handler() {
		if (child)
			delete child;
	}
	virtual std::string name() = 0;
	virtual Grabber::State grab_mode() = 0;
};

void queue(sigc::slot<void> f) {
	if (Handler::idle()) {
		f();
		XFlush(dpy);
	} else
		queued.push_back(f);
}

class OSD : public Gtk::Window {
	static std::list<OSD *> osd_stack;
	int w;
public:
	OSD(Glib::ustring txt) : Gtk::Window(Gtk::WINDOW_POPUP) {
		osd_stack.push_back(this);
		set_accept_focus(false);
		set_border_width(15);
		WIDGET(Gtk::Label, label, "<big><b>" + txt + "</b></big>");
		label.set_use_markup();
		label.modify_fg(Gtk::STATE_NORMAL, Gdk::Color("White"));
		modify_bg(Gtk::STATE_NORMAL, Gdk::Color("RoyalBlue3"));
		set_opacity(0.75);
		add(label);
		label.show();
		int h;
		get_size(w,h);
		do_move();
		show();
		get_window()->input_shape_combine_region(Gdk::Region(), 0, 0);
	}
	static void do_move() {
		int left = gdk_screen_width() - 10;
		for (std::list<OSD *>::iterator i = osd_stack.begin(); i != osd_stack.end(); i++) {
			left -= (*i)->w + 30;
			(*i)->move(left, 40);
		}
	}
	virtual ~OSD() {
		osd_stack.remove(this);
		do_move();
	}
};

std::list<OSD *> OSD::osd_stack;

class IgnoreHandler : public Handler {
	RModifiers mods;
public:
	IgnoreHandler(RModifiers mods_) : mods(mods_) {}
	virtual void press(guint b, RTriple e) {
		if (current_dev->master) {
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), e->x, e->y, 0);
			XTestFakeButtonEvent(dpy, b, true, CurrentTime);
		}
	}
	virtual void motion(RTriple e) {
		if (current_dev->master)
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), e->x, e->y, 0);
	}
	// TODO: Handle Proximity
	virtual void release(guint b, RTriple e) {
		if (current_dev->master) {
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), e->x, e->y, 0);
			XTestFakeButtonEvent(dpy, b, false, CurrentTime);
		}
		if (!xinput_pressed.size())
			parent->replace_child(NULL);
	}
	virtual std::string name() { return "Ignore"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

class ButtonHandler : public Handler {
	RModifiers mods;
	guint button, real_button;
public:
	ButtonHandler(RModifiers mods_, guint button_) : mods(mods_), button(button_), real_button(0) {}
	virtual void press(guint b, RTriple e) {
		if (current_dev->master) {
			if (!real_button)
				real_button = b;
			if (real_button == b)
				b = button;
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), e->x, e->y, 0);
			XTestFakeButtonEvent(dpy, b, true, CurrentTime);
		}
	}
	virtual void motion(RTriple e) {
		if (current_dev->master)
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), e->x, e->y, 0);
	}
	// TODO: Handle Proximity
	virtual void release(guint b, RTriple e) {
		if (current_dev->master) {
			if (real_button == b)
				b = button;
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), e->x, e->y, 0);
			XTestFakeButtonEvent(dpy, b, false, CurrentTime);
		}
		if (!xinput_pressed.size())
			parent->replace_child(NULL);
	}
	virtual std::string name() { return "Button"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

static void bail_out();

static void bail_out() {
	handler->replace_child(NULL);
	xinput_pressed.clear();
	XFlush(dpy);
}

static int xErrorHandler(Display *dpy2, XErrorEvent *e) {
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
			(grabber && e->request_code == grabber->event &&
			 e->minor_code == X_GrabDeviceButton)) {
		if (!handler || Handler::idle()) {
			printf(_("Error: %s\n"), e->request_code==X_GrabButton ? "A grab failed" : "An XInput grab failed");
		} else {
			printf(_("Error: A grab failed.  Resetting...\n"));
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

static int xIOErrorHandler(Display *dpy2) {
	if (dpy != dpy2)
		return oldIOHandler(dpy2);
	printf(_("Fatal Error: Connection to X server lost, restarting...\n"));
	char *args[argc+1];
	for (int i = 0; i<argc; i++)
		args[i] = argv[i];
	args[argc] = NULL;
	execv(argv[0], args);
	return 0;
}

static XAtom EASYSTROKE_PING("EASYSTROKE_PING");
static void ping() {
	XClientMessageEvent ev;
	ev.type = ClientMessage;
	ev.window = ping_window;
	ev.message_type = *EASYSTROKE_PING;
	ev.format = 32;
	XSendEvent(dpy, ping_window, False, 0, (XEvent *)&ev);
	XFlush(dpy);
}

class WaitForPongHandler : public Handler, protected Timeout {
public:
	WaitForPongHandler() { set_timeout(100); }
	virtual void timeout() {
		printf("Warning: %s timed out\n", "WaitForPongHandler");
		bail_out();
	}
	virtual void pong() { parent->replace_child(NULL); }
	virtual std::string name() { return "WaitForPong"; }
	virtual Grabber::State grab_mode() { return parent->grab_mode(); }
};

static void update_core_mapping() {
	unsigned char map[MAX_BUTTONS];
	int n = XGetPointerMapping(dpy, map, MAX_BUTTONS);
	core_inv_map.clear();
	for (int i = n-1; i; i--)
		if (map[i] == i+1)
			core_inv_map.erase(i+1);
		else
			core_inv_map[map[i]] = i+1;
}

static inline float abs(float x) { return x > 0 ? x : -x; }

class AbstractScrollHandler : public Handler {
	bool have_x, have_y;
	float last_x, last_y;
	Time last_t;
	float offset_x, offset_y;
	Glib::ustring str;
	int orig_x, orig_y;

protected:
	AbstractScrollHandler() : last_t(0), offset_x(0.0), offset_y(0.0) {
		if (!prefs.move_back.get() || current_dev->absolute)
			return;
		Window dummy1, dummy2;
		int dummy3, dummy4;
		unsigned int dummy5;
		XQueryPointer(dpy, ROOT, &dummy1, &dummy2, &orig_x, &orig_y, &dummy3, &dummy4, &dummy5);
	}
	virtual void fake_wheel(int b1, int n1, int b2, int n2) {
		for (int i = 0; i<n1; i++)
			fake_click(b1);
		for (int i = 0; i<n2; i++)
			fake_click(b2);
	}
	static float curve(float v) {
		return v * exp(log(abs(v))/3);
	}
protected:
	void move_back() {
		if (!prefs.move_back.get() || current_dev->absolute)
			return;
		XTestFakeMotionEvent(dpy, DefaultScreen(dpy), orig_x, orig_y, 0);
	}
public:
	virtual void raw_motion(RTriple e, bool abs_x, bool abs_y) {
		float dx = abs_x ? (have_x ? e->x - last_x : 0) : e->x;
		float dy = abs_y ? (have_y ? e->y - last_y : 0) : e->y;

		if (abs_x) {
			last_x = e->x;
			have_x = true;
		}

		if (abs_y) {
			last_y = e->y;
			have_y = true;
		}

		if (!last_t) {
			last_t = e->t;
			return;
		}

		if (e->t == last_t)
			return;

		int dt = e->t - last_t;
		last_t = e->t;

		double factor = (prefs.scroll_invert.get() ? 1.0 : -1.0) * prefs.scroll_speed.get();
		offset_x += factor * curve(dx/dt)*dt/20.0;
		offset_y += factor * curve(dy/dt)*dt/10.0;
		int b1 = 0, n1 = 0, b2 = 0, n2 = 0;
		if (abs(offset_x) > 1.0) {
			n1 = (int)floor(abs(offset_x));
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
			n2 = (int)floor(abs(offset_y));
			if (offset_y > 0) {
				b2 = 5;
				offset_y -= n2;
			} else {
				b2 = 4;
				offset_y += n2;
			}
		}
		if (n1 || n2)
			fake_wheel(b1,n1, b2,n2);
	}
};

class ScrollHandler : public AbstractScrollHandler {
	RModifiers mods;
	std::map<guint, guint> map;
	int orig_x, orig_y;
public:
	ScrollHandler(RModifiers mods_) : mods(mods_) {
		// If you want to use buttons > 9, you're on your own..
		map[1] = 0; map[2] = 0; map[3] = 0; map[8] = 0; map[9] = 0;
	}
	virtual void raw_motion(RTriple e, bool abs_x, bool abs_y) {
		if (xinput_pressed.size())
			AbstractScrollHandler::raw_motion(e, abs_x, abs_y);
	}
	virtual void press_master(guint b, Time t) {
		fake_core_button(b, false);
	}
	virtual void release(guint b, RTriple e) {
		if (in_proximity || xinput_pressed.size())
			return;
		parent->replace_child(0);
		move_back();
	}
	virtual void proximity_out() {
		parent->replace_child(0);
		move_back();
	}
	virtual std::string name() { return "Scroll"; }
	virtual Grabber::State grab_mode() { return Grabber::RAW; }
};

class ScrollAdvancedHandler : public AbstractScrollHandler {
	RModifiers m;
	guint &rb;
public:
	ScrollAdvancedHandler(RModifiers m_, guint &rb_) : m(m_), rb(rb_) {}
	virtual void fake_wheel(int b1, int n1, int b2, int n2) {
		AbstractScrollHandler::fake_wheel(b1, n1, b2, n2);
		rb = 0;
	}
	virtual void release(guint b, RTriple e) {
		Handler *p = parent;
		p->replace_child(NULL);
		p->release(b, e);
		move_back();
	}
	virtual void press(guint b, RTriple e) {
		Handler *p = parent;
		p->replace_child(NULL);
		p->press(b, e);
		move_back();
	}
	virtual std::string name() { return "ScrollAdvanced"; }
	virtual Grabber::State grab_mode() { return Grabber::RAW; }
};

// Hack so that we don't have to move stuff around so much
static bool mods_equal(RModifiers m1, RModifiers m2);

class AdvancedStrokeActionHandler : public Handler {
	RStroke s;
public:
	AdvancedStrokeActionHandler(RStroke s_, RTriple e) : s(s_) {}
	virtual void press(guint b, RTriple e) {
		if (stroke_action) {
			s->button = b;
			(*stroke_action)(s);
		}
	}
	virtual void release(guint b, RTriple e) {
		if (stroke_action)
			(*stroke_action)(s);
		if (xinput_pressed.size() == 0)
			parent->replace_child(NULL);
	}
	virtual std::string name() { return "InstantStrokeAction"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

class AdvancedHandler : public Handler {
	RTriple e;
	guint remap_from, remap_to;
	Time click_time;
	guint replay_button;
	RTriple replay_orig;
	std::map<guint, RAction> as;
	std::map<guint, RRanking> rs;
	std::map<guint, RModifiers> mods;
	RModifiers sticky_mods;
	guint button1, button2;
	RPreStroke replay;

	void show_ranking(guint b, RTriple e) {
		if (!rs.count(b))
			return;
		Ranking::queue_show(rs[b], e);
		rs.erase(b);
	}
	AdvancedHandler(RStroke s, RTriple e_, guint b1, guint b2, RPreStroke replay_) :
		e(e_), remap_from(0), remap_to(0), click_time(0), replay_button(0),
		button1(b1), button2(b2), replay(replay_) {
			if (s)
				actions.get_action_list(grabber->current_class->get())->handle_advanced(s, as, rs, b1, b2);
		}
public:
	static Handler *create(RStroke s, RTriple e, guint b1, guint b2, RPreStroke replay) {
		if (stroke_action && s)
			return new AdvancedStrokeActionHandler(s, e);
		else
			return new AdvancedHandler(s, e, b1, b2, replay);

	}
	virtual void init() {
		if (replay && replay->size()) {
			bool replay_first = !as.count(button2);
			PreStroke::iterator i = replay->begin();
			if (replay_first)
				press(button2 ? button2 : button1, *i);
			while (i != replay->end())
				motion(*i++);
			if (!replay_first)
				press(button2 ? button2 : button1, e);
		} else {
			press(button2 ? button2 : button1, e);
		}
		replay.reset();
	}
	virtual void press(guint b, RTriple e) {
		if (current_dev->master)
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), e->x, e->y, 0);
		click_time = 0;
		if (remap_to) {
			fake_core_button(remap_to, false);
		}
		remap_from = 0;
		remap_to = 0;
		replay_button = 0;
		guint bb = (b == button1) ? button2 : b;
		show_ranking(bb, e);
		if (!as.count(bb)) {
			sticky_mods.reset();
			if (current_dev->master)
				XTestFakeButtonEvent(dpy, b, true, CurrentTime);
			return;
		}
		RAction act = as[bb];
		if (IS_SCROLL(act)) {
			click_time = e->t;
			replay_button = b;
			replay_orig = e;
			RModifiers m = act->prepare();
			sticky_mods.reset();
			return replace_child(new ScrollAdvancedHandler(m, replay_button));
		}
		if (IS_IGNORE(act)) {
			click_time = e->t;
			replay_button = b;
			replay_orig = e;
		}
		IF_BUTTON(act, b2) {
			// This is kind of a hack:  Store modifiers in
			// sticky_mods, so that they are automatically released
			// on the next press
			sticky_mods = act->prepare();
			remap_from = b;
			remap_to = b2;
			fake_core_button(b2, true);
			return;
		}
		mods[b] = act->prepare();
		if (IS_KEY(act)) {
			if (mods_equal(sticky_mods, mods[b]))
				mods[b] = sticky_mods;
			else
				sticky_mods = mods[b];
		} else
			sticky_mods.reset();
		act->run();
	}
	virtual void motion(RTriple e) {
		if (replay_button && hypot(replay_orig->x - e->x, replay_orig->y - e->y) > 16)
			replay_button = 0;
		if (current_dev->master)
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), e->x, e->y, 0);
	}
	virtual void release(guint b, RTriple e) {
		if (current_dev->master)
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), e->x, e->y, 0);
		if (remap_to) {
			fake_core_button(remap_to, false);
		}
		guint bb = (b == button1) ? button2 : b;
		if (!as.count(bb)) {
			sticky_mods.reset();
			if (current_dev->master)
				XTestFakeButtonEvent(dpy, b, false, CurrentTime);
		}
		if (xinput_pressed.size() == 0) {
			if (e->t < click_time + 250 && b == replay_button) {
				sticky_mods.reset();
				mods.clear();
				fake_click(b);
			}
			return parent->replace_child(NULL);
		}
		replay_button = 0;
		mods.erase((b == button1) ? button2 : b);
		if (remap_from)
			sticky_mods.reset();
		remap_from = 0;
	}
	virtual std::string name() { return "Advanced"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

Atom get_atom(Window w, Atom prop) {
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop_return = NULL;

	if (XGetWindowProperty(dpy, w, prop, 0, sizeof(Atom), False, XA_ATOM, &actual_type, &actual_format,
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

	if (XGetWindowProperty(dpy, w, prop, 0, sizeof(Atom), False, XA_ATOM, &actual_type, &actual_format,
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

	if (XGetWindowProperty(dpy, w, prop, 0, sizeof(Atom), False, XA_WINDOW, &actual_type, &actual_format,
				&nitems, &bytes_after, &prop_return) != Success)
		return None;
	if (!prop_return)
		return None;
	Window ret = *(Window *)prop_return;
	XFree(prop_return);
	return ret;
}

static void icccm_client_message(Window w, Atom a, Time t) {
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

static void activate_window(Window w, Time t) {
	static XAtom _NET_ACTIVE_WINDOW("_NET_ACTIVE_WINDOW");
	static XAtom _NET_WM_WINDOW_TYPE("_NET_WM_WINDOW_TYPE");
	static XAtom _NET_WM_WINDOW_TYPE_DOCK("_NET_WM_WINDOW_TYPE_DOCK");
	static XAtom WM_PROTOCOLS("WM_PROTOCOLS");
	static XAtom WM_TAKE_FOCUS("WM_TAKE_FOCUS");

	if (w == get_window(ROOT, *_NET_ACTIVE_WINDOW))
		return;

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

	if (!has_atom(w, *WM_PROTOCOLS, *WM_TAKE_FOCUS))
		return;

	XWindowAttributes attr;
	if (XGetWindowAttributes(dpy, w, &attr) && attr.override_redirect)
		return;

	if (verbosity >= 3)
		printf("Giving focus to window 0x%lx\n", w);

	icccm_client_message(w, *WM_TAKE_FOCUS, t);
}

void Trace::start(Trace::Point p) {
	last = p;
	active = true;
	XFixesHideCursor(dpy, ROOT);
	start_();
}

void Trace::end() {
	if (!active)
		return;
	active = false;
	XFixesShowCursor(dpy, ROOT);
	end_();
}

static void get_timeouts(TimeoutType type, int *init, int *final) {
	switch (type) {
		case TimeoutOff:
			*init = 0;
			*final = 0;
			break;
		case TimeoutConservative:
			*init = 750;
			*final = 750;
			break;
		case TimeoutDefault:
			*init = 250;
			*final = 250;
			break;
		case TimeoutMedium:
			*init = 100;
			*final = 100;
			break;
		case TimeoutAggressive:
			*init = 50;
			*final = 75;
			break;
		case TimeoutFlick:
			*init = 30;
			*final = 50;
			break;
		default:;
	}
}

class StrokeHandler : public Handler, public sigc::trackable {
	guint button;
	RPreStroke cur;
	bool is_gesture;
	bool drawing;
	RTriple last, orig;
	bool use_timeout;
	int init_timeout, final_timeout, radius;
	struct Connection {
		sigc::connection c;
		double dist;
		Connection(StrokeHandler *parent, double dist_, int to) : dist(dist_) {
			c = Glib::signal_timeout().connect(sigc::mem_fun(*parent, &StrokeHandler::timeout), to);
		}
		~Connection() { c.disconnect(); }
	};
	typedef boost::shared_ptr<Connection> RConnection;
	sigc::connection init_connection;
	std::vector<RConnection> connections;

	RStroke finish(guint b) {
		trace->end();
		XFlush(dpy);
		RPreStroke c = cur;
		if (!is_gesture || grabber->is_instant(button))
			c.reset(new PreStroke);
		if (b && prefs.advanced_ignore.get())
			c.reset(new PreStroke);
		return Stroke::create(*c, button, b, false);
	}

	bool timeout() {
		if (verbosity >= 2)
			printf("Aborting stroke...\n");
		trace->end();
		RPreStroke c = cur;
		if (!is_gesture)
			c.reset(new PreStroke);
		RStroke s;
		if (prefs.timeout_gestures.get() || grabber->is_click_hold(button))
			s = Stroke::create(*c, button, 0, true);
		parent->replace_child(AdvancedHandler::create(s, last, button, 0, cur));
		XFlush(dpy);
		return false;
	}

	void do_instant() {
		PreStroke ps;
		RStroke s = Stroke::create(ps, button, button, false);
		parent->replace_child(AdvancedHandler::create(s, orig, button, button, cur));
	}

	bool expired(RConnection c, double dist) {
		c->dist -= dist;
		return c->dist < 0;
	}
protected:
	void abort_stroke() {
		parent->replace_child(AdvancedHandler::create(RStroke(), last, button, 0, cur));
	}
	virtual void pressure() {
		abort_stroke();
		timeout();
	}
	virtual void motion(RTriple e) {
		cur->add(e);
		float dist = hypot(e->x-orig->x, e->y-orig->y);
		if (!is_gesture && dist > 16) {
			if (use_timeout && !final_timeout)
				return abort_stroke();
			init_connection.disconnect();
			is_gesture = true;
		}
		if (!drawing && dist > 4 && (!use_timeout || final_timeout)) {
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
		if (use_timeout && is_gesture) {
			connections.erase(remove_if(connections.begin(), connections.end(),
						sigc::bind(sigc::mem_fun(*this, &StrokeHandler::expired),
							hypot(e->x - last->x, e->y - last->y))), connections.end());
			connections.push_back(RConnection(new Connection(this, radius, final_timeout)));
		}
		last = e;
	}

	virtual void press(guint b, RTriple e) {
		RStroke s = finish(b);
		parent->replace_child(AdvancedHandler::create(s, e, button, b, cur));
	}

	virtual void release(guint b, RTriple e) {
		RStroke s = finish(0);

		if (prefs.move_back.get() && !current_dev->absolute)
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), orig->x, orig->y, 0);
		else
			XTestFakeMotionEvent(dpy, DefaultScreen(dpy), e->x, e->y, 0);

		if (stroke_action) {
			(*stroke_action)(s);
			return parent->replace_child(NULL);
		}
		RRanking ranking;
		RAction act = actions.get_action_list(grabber->current_class->get())->handle(s, ranking);
		if (!IS_CLICK(act))
			Ranking::queue_show(ranking, e);
		if (!act) {
			XBell(dpy, 0);
			return parent->replace_child(NULL);
		}
		RModifiers mods = act->prepare();
		if (IS_CLICK(act))
			act = Button::create((Gdk::ModifierType)0, b);
		else IF_BUTTON(act, b)
			return parent->replace_child(new ButtonHandler(mods, b));
		if (IS_IGNORE(act))
			return parent->replace_child(new IgnoreHandler(mods));
		if (IS_SCROLL(act))
			return parent->replace_child(new ScrollHandler(mods));
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", (int)orig->x);
		setenv("EASYSTROKE_X1", buf, 1);
		snprintf(buf, sizeof(buf), "%d", (int)orig->y);
		setenv("EASYSTROKE_Y1", buf, 1);
		snprintf(buf, sizeof(buf), "%d", (int)e->x);
		setenv("EASYSTROKE_X2", buf, 1);
		snprintf(buf, sizeof(buf), "%d", (int)e->y);
		setenv("EASYSTROKE_Y2", buf, 1);
		act->run();
		unsetenv("EASYSTROKE_X1");
		unsetenv("EASYSTROKE_Y1");
		unsetenv("EASYSTROKE_X2");
		unsetenv("EASYSTROKE_Y2");
		parent->replace_child(NULL);
	}
public:
	StrokeHandler(guint b, RTriple e) : button(b), is_gesture(false), drawing(false), last(e), orig(e),
	init_timeout(prefs.init_timeout.get()), final_timeout(prefs.final_timeout.get()), radius(16) {
		const std::map<std::string, TimeoutType> &dt = prefs.device_timeout.ref();
		std::map<std::string, TimeoutType>::const_iterator j = dt.find(current_dev->name);
		if (j != dt.end())
			get_timeouts(j->second, &init_timeout, &final_timeout);
		else
			get_timeouts(prefs.timeout_profile.get(), &init_timeout, &final_timeout);
		use_timeout = init_timeout;
	}
	virtual void init() {
		if (grabber->is_instant(button))
			return do_instant();
		if (grabber->is_click_hold(button)) {
			use_timeout = true;
			init_timeout = 500;
			final_timeout = 0;
		}
		cur = PreStroke::create();
		cur->add(orig);
		if (!use_timeout)
			return;
		if (final_timeout && final_timeout < 32 && radius < 16*32/final_timeout) {
			radius = 16*32/final_timeout;
			final_timeout = final_timeout*radius/16;
		}
		init_connection = Glib::signal_timeout().connect(
				sigc::mem_fun(*this, &StrokeHandler::timeout), init_timeout);
	}
	~StrokeHandler() { trace->end(); }
	virtual std::string name() { return "Stroke"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

class IdleHandler : public Handler {
protected:
	virtual void init() {
		update_core_mapping();
	}
	virtual void press(guint b, RTriple e) {
		if (current_app_window.get())
			activate_window(current_app_window.get(), e->t);
		replace_child(new StrokeHandler(b, e));
	}
public:
	virtual ~IdleHandler() {
		XUngrabKey(dpy, XKeysymToKeycode(dpy,XK_Escape), AnyModifier, ROOT);
	}
	virtual std::string name() { return "Idle"; }
	virtual Grabber::State grab_mode() { return Grabber::BUTTON; }
};

class SelectHandler : public Handler {
	virtual void press_master(guint b, Time t) {
		parent->replace_child(new WaitForPongHandler);
		ping();
		queue(sigc::ptr_fun(&gtk_main_quit));
	}
public:
	static void create() {
		win->get_window().get_window()->lower();
		handler->top()->replace_child(new SelectHandler);
	}
	virtual std::string name() { return "Select"; }
	virtual Grabber::State grab_mode() { return Grabber::SELECT; }
};

static void do_run_by_name(RAction act) {
	RModifiers mods = act->prepare();
	if (IS_IGNORE(act))
		return handler->replace_child(new IgnoreHandler(mods));
	if (IS_SCROLL(act))
		return handler->replace_child(new ScrollHandler(mods));
	act->run();
}

void run_by_name(const char *str) {
	if (!strcmp(str, "")) {
		win->show_hide();
		return;
	}
	for (ActionDB::const_iterator i = actions.begin(); i != actions.end(); i++) {
		if (i->second.name == std::string(str)) {
			if (i->second.action)
				queue(sigc::bind(sigc::ptr_fun(do_run_by_name), i->second.action));
			return;
		}
	}
	printf(_("Warning: No action \"%s\" defined\n"), str);
}

void icon_warning() {
	for (ActionDB::const_iterator i = actions.begin(); i != actions.end(); i++) {
		Misc *m = dynamic_cast<Misc *>(i->second.action.get());
		if (m && m->type == Misc::SHOWHIDE)
			return;
	}
	if (!win)
		return;

	Gtk::MessageDialog *md;
	widgets->get_widget("dialog_icon", md);
	md->set_message(_("Tray icon disabled"));
	md->set_secondary_text(_("To bring the configuration dialog up again, you should define an action of type Misc...Show/Hide."));
	md->run();
	md->hide();
}

void quit() {
	static bool dead = false;
	if (dead)
		bail_out();
	dead = true;
	queue(sigc::ptr_fun(&Gtk::Main::quit));
}

static void quit(int) { quit(); }

class Main {
	std::string parse_args_and_init_gtk();
	void create_config_dir();
	char* next_event();
	void usage(char *me, bool good);
	void version();

	std::string display;
	Gtk::Main *kit;
public:
	Main();
	void run();
	bool handle(Glib::IOCondition);
	void handle_enter_leave(XEvent &ev);
	void handle_event(XEvent &ev);
	void handle_xi2_event(XIDeviceEvent *event);
	void handle_raw_motion(XIRawEvent *event);
	void report_xi2_event(XIDeviceEvent *event, const char *type);
	~Main();
};

class ReloadTrace : public Timeout {
	void timeout() {
		if (verbosity >= 2)
			printf("Reloading gesture display\n");
		queue(sigc::mem_fun(*this, &ReloadTrace::reload));
	}
	void reload() { trace.reset(init_trace()); }
} reload_trace;

static void schedule_reload_trace() { reload_trace.set_timeout(1000); }

static void xdg_open(const Glib::ustring str) {
	if (!fork()) {
		execlp("xdg-open", "xdg-open", str.c_str(), NULL);
		exit(EXIT_FAILURE);
	}
}

static void link_button_hook(Gtk::LinkButton *, const Glib::ustring& uri) { xdg_open(uri); }
static void about_dialog_hook(Gtk::AboutDialog &, const Glib::ustring& url) { xdg_open(url); }

// dbus-send --type=method_call --dest=org.easystroke /org/easystroke org.easystroke.send string:"foo"
static void send_dbus(bool action, const char *str) {
	GError *error = 0;
	DBusGConnection *bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (!bus) {
		printf(_("Error initializing D-BUS\n"));
		exit(EXIT_FAILURE);
	}
	DBusGProxy *proxy = dbus_g_proxy_new_for_name(bus, "org.easystroke", "/org/easystroke", "org.easystroke");
	if (action)
		dbus_g_proxy_call_no_reply(proxy, "send", G_TYPE_STRING, str, G_TYPE_INVALID);
	else
		dbus_g_proxy_call_no_reply(proxy, str, G_TYPE_INVALID);
}

int start_dbus();

Main::Main() : kit(0) {
	bindtextdomain("easystroke", is_dir("po") ? "po" : LOCALEDIR);
	bind_textdomain_codeset("easystroke", "UTF-8");
	textdomain("easystroke");
	if (0) {
		RStroke trefoil = Stroke::trefoil();
		trefoil->draw_svg("easystroke.svg");
		exit(EXIT_SUCCESS);
	}
	if (argc > 1 && !strcmp(argv[1], "send")) {
		if (argc == 2)
			usage(argv[0], false);
		gtk_init(&argc, &argv);
		send_dbus(true, argv[2]);
		exit(EXIT_SUCCESS);
	}

	if (argc > 1 && (!strcmp(argv[1], "enable") || !strcmp(argv[1], "about") || !strcmp(argv[1], "quit"))) {
		gtk_init(&argc, &argv);
		send_dbus(false, argv[1]);
		exit(EXIT_SUCCESS);
	}

	display = parse_args_and_init_gtk();
	create_config_dir();
	unsetenv("DESKTOP_AUTOSTART_ID");

	signal(SIGINT, &quit);
	signal(SIGCHLD, SIG_IGN);

	Gtk::LinkButton::set_uri_hook(sigc::ptr_fun(&link_button_hook));
	Gtk::AboutDialog::set_url_hook(sigc::ptr_fun(&about_dialog_hook));

	dpy = XOpenDisplay(display.c_str());
	if (!dpy) {
		printf(_("Couldn't open display.\n"));
		exit(EXIT_FAILURE);
	}
	if (!no_dbus && start_dbus() < 0) {
		printf(_("Easystroke is already running, showing configuration window instead.\n"));
		send_dbus(true, "");
		exit(EXIT_SUCCESS);
	}
	ROOT = DefaultRootWindow(dpy);

	ping_window = XCreateSimpleWindow(dpy, ROOT, 0, 0, 1, 1, 0, 0, 0);

	prefs.init();
	action_watcher = new ActionDBWatcher;
	action_watcher->init();

	grabber = new Grabber;
	// Force enter events to be generated
	XGrabPointer(dpy, ROOT, False, 0, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	XUngrabPointer(dpy, CurrentTime);

	trace.reset(init_trace());
	Glib::RefPtr<Gdk::Screen> screen = Gdk::Display::get_default()->get_default_screen();
	g_signal_connect(screen->gobj(), "composited-changed", &schedule_reload_trace, NULL);
	screen->signal_size_changed().connect(sigc::ptr_fun(&schedule_reload_trace));
	Notifier *trace_notify = new Notifier(sigc::ptr_fun(&schedule_reload_trace));
	prefs.trace.connect(trace_notify);
	prefs.color.connect(trace_notify);

	handler = new IdleHandler;
	handler->init();
	XTestGrabControl(dpy, True);

}

extern const char *gui_buffer;

void Main::run() {
	Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(dpy), Glib::IO_IN);
	io->connect(sigc::mem_fun(*this, &Main::handle));
	io->attach();
	try {
		widgets = Gtk::Builder::create_from_string(gui_buffer);
	} catch (Gtk::BuilderError &e) {
		printf("Error building GUI: %s\n", e.what().c_str());
		exit(EXIT_FAILURE);
	}
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
	printf("       %s enable\n", me);
	printf("       %s about\n", me);
	printf("       %s quit\n", me);
	printf("\n");
	printf("Options:\n");
	printf("  -c, --config-dir       Directory for config files\n");
	printf("      --display          X Server to contact\n");
	printf("  -D  --no-dbus          Don't try to register as a DBus service\n");
	printf("  -e  --experimental     Start in experimental mode\n");
	printf("  -g, --show-gui         Show the configuration dialog on startup\n");
	printf("  -x  --disable          Start disabled\n");
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

std::string Main::parse_args_and_init_gtk() {
	static struct option long_opts1[] = {
		{"display",1,0,'d'},
		{"help",0,0,'h'},
		{"version",0,0,'V'},
		{"show-gui",0,0,'g'},
		{0,0,0,0}
	};
	static struct option long_opts2[] = {
		{"config-dir",1,0,'c'},
		{"display",1,0,'d'},
		{"experimental",0,0,'e'},
		{"show-gui",0,0,'g'},
		{"verbose",0,0,'v'},
		{"no-dbus",0,0,'D'},
		{"disabled",0,0,'x'},
		{0,0,0,0}
	};
	std::string display;
	int opt;
	// parse --display here, before Gtk::Main(...) takes it away from us
	opterr = 0;
	while ((opt = getopt_long(argc, argv, "gh", long_opts1, 0)) != -1)
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
		}
	optind = 1;
	opterr = 1;
	kit = new Gtk::Main(argc, argv);
	oldHandler = XSetErrorHandler(xErrorHandler);
	oldIOHandler = XSetIOErrorHandler(xIOErrorHandler);

	while ((opt = getopt_long(argc, argv, "c:egvDx", long_opts2, 0)) != -1) {
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
			case 'D':
				no_dbus = true;
				break;
			case 'x':
				disabled.set(true);
				break;
			case 'd':
			case 'n':
			case 'g':
				break;
			default:
				usage(argv[0], false);
		}
	}
	return display;
}

void Main::create_config_dir() {
	if (config_dir == "") {
		config_dir = getenv("HOME");
		config_dir += "/.easystroke";
	}
	struct stat st;
	char *name = canonicalize_file_name(config_dir.c_str());

	// check if the directory does not exist
	if (lstat(name, &st) == -1) {
		if (mkdir(config_dir.c_str(), 0777) == -1) {
			printf(_("Error: Couldn't create configuration directory \"%s\"\n"), config_dir.c_str());
			exit(EXIT_FAILURE);
		}
	} else {
		if (!S_ISDIR(st.st_mode)) {
			printf(_("Error: \"%s\" is not a directory\n"), config_dir.c_str());
			exit(EXIT_FAILURE);
		}


	}
	free (name);
	config_dir += "/";
}

extern Window get_app_window(Window w);

void Main::handle_enter_leave(XEvent &ev) {
	if (ev.xcrossing.mode == NotifyGrab)
		return;
	if (ev.xcrossing.detail == NotifyInferior)
		return;
	Window w = ev.xcrossing.window;
	if (ev.type == EnterNotify) {
		current_app_window.set(get_app_window(w));
		if (verbosity >= 3)
			printf("Entered window 0x%lx -> 0x%lx\n", w, current_app_window.get());
	} else printf("Error: Bogus Enter/Leave event\n");
}

#define H (handler->top())
void Main::handle_event(XEvent &ev) {

	switch(ev.type) {
	case EnterNotify:
	case LeaveNotify:
		handle_enter_leave(ev);
		return;

	case PropertyNotify:
		if (current_app_window.get() == ev.xproperty.window && ev.xproperty.atom == XA_WM_CLASS)
			current_app_window.notify();
		return;

	case ButtonPress:
		if (verbosity >= 3)
			printf("Press (master): %d (%d, %d) at t = %ld\n", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y, ev.xbutton.time);
			H->press_master(ev.xbutton.button, ev.xbutton.time);
		return;

	case ClientMessage:
		if (ev.xclient.window != ping_window)
			return;
		if (ev.xclient.message_type == *EASYSTROKE_PING) {
			if (verbosity >= 3)
				printf("Pong\n");
			H->pong();
		}
		return;

	case MappingNotify:
		if (ev.xmapping.request == MappingPointer)
			update_core_mapping();
		if (ev.xmapping.request == MappingKeyboard || ev.xmapping.request == MappingModifier)
			XRefreshKeyboardMapping(&ev.xmapping);
		return;
	case GenericEvent:
		if (ev.xcookie.extension == grabber->opcode && XGetEventData(dpy, &ev.xcookie)) {
			handle_xi2_event((XIDeviceEvent *)ev.xcookie.data);
			XFreeEventData(dpy, &ev.xcookie);
		}
	}
}

static void print_coordinates(XIValuatorState *valuators, double *values) {
	int n = 0;
	for (int i = valuators->mask_len - 1; i >= 0; i--)
		if (XIMaskIsSet(valuators->mask, i)) {
			n = i+1;
			break;
		}
	bool first = true;
	int elt = 0;
	for (int i = 0; i < n; i++) {
		if (first)
			first = false;
		else
			printf(", ");
		if (XIMaskIsSet(valuators->mask, i))
			printf("%.3f", values[elt++]);
		else
			printf("*");
	}
}

void Main::report_xi2_event(XIDeviceEvent *event, const char *type) {
	printf("%s (XI2): ", type);
	if (event->detail)
		printf("%d ", event->detail);
	printf("(%.3f, %.3f) - (", event->root_x, event->root_y);
	print_coordinates(&event->valuators, event->valuators.values);
	printf(") at t = %ld\n", event->time);
}

void Main::handle_xi2_event(XIDeviceEvent *event) {
	switch (event->evtype) {
		case XI_ButtonPress:
			if (verbosity >= 3)
				report_xi2_event(event, "Press");
			if (xinput_pressed.size()) {
				if (!current_dev || current_dev->dev != event->deviceid)
					break;
			}
			current_dev = grabber->get_xi_dev(event->deviceid);
			if (!current_dev) {
				printf("Warning: Spurious device event\n");
				break;
			}
			if (current_dev->master)
				XISetClientPointer(dpy, None, current_dev->master);
			xinput_pressed.insert(event->detail);
			H->press(event->detail, create_triple(event->root_x, event->root_y, event->time));
			break;
		case XI_ButtonRelease:
			if (verbosity >= 3)
				report_xi2_event(event, "Release");
			if (!current_dev || current_dev->dev != event->deviceid)
				break;
			xinput_pressed.erase(event->detail);
			H->release(event->detail, create_triple(event->root_x, event->root_y, event->time));
			break;
		case XI_Motion:
			if (verbosity >= 5)
				report_xi2_event(event, "Motion");
			if (!current_dev || current_dev->dev != event->deviceid)
				break;
			if (current_dev->supports_pressure && XIMaskIsSet(event->valuators.mask, 2)) {
				int i = 0;
				if (XIMaskIsSet(event->valuators.mask, 0))
				       i++;
				if (XIMaskIsSet(event->valuators.mask, 1))
				       i++;
				int z = current_dev->normalize_pressure(event->valuators.values[i]);
				if (prefs.pressure_abort.get() && z >= prefs.pressure_threshold.get())
					H->pressure();
			}
			H->motion(create_triple(event->root_x, event->root_y, event->time));
			break;
		case XI_RawMotion:
			handle_raw_motion((XIRawEvent *)event);
			break;
		case XI_HierarchyChanged:
			grabber->hierarchy_changed((XIHierarchyEvent *)event);
	}
}

void Main::handle_raw_motion(XIRawEvent *event) {
	if (!current_dev || current_dev->dev != event->deviceid)
		return;
	double x = 0.0, y = 0.0;
	bool abs_x = current_dev->absolute;
	bool abs_y = current_dev->absolute;
	int i = 0;

	if (XIMaskIsSet(event->valuators.mask, 0))
		x = event->raw_values[i++];
	else
		abs_x = false;

	if (XIMaskIsSet(event->valuators.mask, 1))
		y = event->raw_values[i++];
	else
		abs_y = false;

	if (verbosity >= 5) {
		printf("Raw motion (XI2): (");
		print_coordinates(&event->valuators, event->raw_values);
		printf(") at t = %ld\n", event->time);
	}

	H->raw_motion(create_triple(x * current_dev->scale_x, y * current_dev->scale_y, event->time), abs_x, abs_y);
}

#undef H

bool Main::handle(Glib::IOCondition) {
	while (XPending(dpy)) {
		try {
			XEvent ev;
			XNextEvent(dpy, &ev);
			if (!grabber->handle(ev))
				handle_event(ev);
		} catch (GrabFailedException &e) {
			printf(_("Error: %s\n"), e.what());
			bail_out();
		}
	}
	return true;
}

Main::~Main() {
	trace->end();
	trace.reset();
	delete grabber;
	delete kit;
	XCloseDisplay(dpy);
	prefs.execute_now();
	action_watcher->execute_now();
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

std::string select_window() {
	queue(sigc::ptr_fun(&SelectHandler::create));
	gtk_main();
	win->get_window().raise();
	return grabber->current_class->get();
}

void Button::run() {
	grabber->suspend();
	fake_click(button);
	grabber->resume();
}

void SendKey::run() {
	if (!key)
		return;
	guint code = XKeysymToKeycode(dpy, key);
	XTestFakeKeyEvent(dpy, code, true, 0);
	XTestFakeKeyEvent(dpy, code, false, 0);
}

void fake_unicode(gunichar c) {
	static const KeySym numcode[10] = { XK_0, XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9 };
	static const KeySym hexcode[6] = { XK_a, XK_b, XK_c, XK_d, XK_e, XK_f };

	if (verbosity >= 3) {
		char buf[7];
		buf[g_unichar_to_utf8(c, buf)] = '\0';
		printf("using unicode input for character %s\n", buf);
	}
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Control_L), true, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L), true, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_u), true, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_u), false, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L), false, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Control_L), false, 0);
	char buf[16];
	snprintf(buf, sizeof(buf), "%x", c);
	for (int i = 0; buf[i]; i++)
		if (buf[i] >= '0' && buf[i] <= '9') {
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, numcode[buf[i]-'0']), true, 0);
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, numcode[buf[i]-'0']), false, 0);
		} else if (buf[i] >= 'a' && buf[i] <= 'f') {
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, hexcode[buf[i]-'a']), true, 0);
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, hexcode[buf[i]-'a']), false, 0);
		}
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_space), true, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_space), false, 0);
}

bool fake_char(gunichar c) {
	char buf[16];
	snprintf(buf, sizeof(buf), "U%04X", c);
	KeySym keysym = XStringToKeysym(buf);
	if (keysym == NoSymbol)
		return false;
	KeyCode keycode = XKeysymToKeycode(dpy, keysym);
	if (!keycode)
		return false;
	KeyCode modifier = 0;
	int n;
	KeySym *mapping = XGetKeyboardMapping(dpy, keycode, 1, &n);
	if (mapping[0] != keysym) {
		int i;
		for (i = 1; i < n; i++)
			if (mapping[i] == keysym)
				break;
		if (i == n)
			return false;
		XModifierKeymap *keymap = XGetModifierMapping(dpy);
		modifier = keymap->modifiermap[i];
		XFreeModifiermap(keymap);
	}
	XFree(mapping);
	if (modifier)
		XTestFakeKeyEvent(dpy, modifier, true, 0);
	XTestFakeKeyEvent(dpy, keycode, true, 0);
	XTestFakeKeyEvent(dpy, keycode, false, 0);
	if (modifier)
		XTestFakeKeyEvent(dpy, modifier, false, 0);
	return true;
}

void SendText::run() {
	for (Glib::ustring::iterator i = text.begin(); i != text.end(); i++)
		if (!fake_char(*i))
			fake_unicode(*i);
}

static struct {
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
static int n_modkeys = 10;

class Modifiers : Timeout {
	static std::set<Modifiers *> all;
	static void update_mods() {
		static guint mod_state = 0;
		guint new_state = 0;
		for (std::set<Modifiers *>::iterator i = all.begin(); i != all.end(); i++)
			new_state |= (*i)->mods;
		for (int i = 0; i < n_modkeys; i++) {
			guint mask = modkeys[i].mask;
			if ((mod_state & mask) ^ (new_state & mask))
				XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, modkeys[i].sym), new_state & mask, 0);
		}
		mod_state = new_state;
	}

	guint mods;
	Glib::ustring str;
	OSD *osd;
public:
	Modifiers(guint mods_, Glib::ustring str_) : mods(mods_), str(str_), osd(NULL) {
		if (prefs.show_osd.get())
			set_timeout(150);
		all.insert(this);
		update_mods();
	}
	bool operator==(const Modifiers &m) {
		return mods == m.mods && str == m.str;
	}
	virtual void timeout() {
		osd = new OSD(str);
	}
	~Modifiers() {
		all.erase(this);
		update_mods();
		delete osd;
	}
};
std::set<Modifiers *> Modifiers::all;

RModifiers ModAction::prepare() {
	return RModifiers(new Modifiers(mods, get_label()));
}

RModifiers SendKey::prepare() {
	if (!mods)
		return RModifiers();
	return RModifiers(new Modifiers(mods, ModAction::get_label()));
}

static bool mods_equal(RModifiers m1, RModifiers m2) {
	return m1 && m2 && *m1 == *m2;
}

void Misc::run() {
	switch (type) {
		case SHOWHIDE:
			win->show_hide();
			return;
		case UNMINIMIZE:
			grabber->unminimize();
			return;
		case DISABLE:
			disabled.set(!disabled.get());
			return;
		default:
			return;
	}
}

bool is_file(std::string filename) {
	struct stat st;
	return lstat(filename.c_str(), &st) != -1 && S_ISREG(st.st_mode);
}

bool is_dir(std::string dirname) {
	struct stat st;
	return lstat(dirname.c_str(), &st) != -1 && S_ISDIR(st.st_mode);
}
