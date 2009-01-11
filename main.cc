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
#include <X11/Xproto.h>
// From #include <X11/extensions/XIproto.h>
// which is not C++-safe
#define X_GrabDeviceButton              17

#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>

const char *versions[] = { "-0.4.0", "", NULL };

bool show_gui = false;
extern bool no_xi;
extern bool xi_15;
bool rotated = false;
bool experimental = false;
int verbosity = 0;
int offset_x = 0;
int offset_y = 0;

Display *dpy;
int argc;
char **argv;

std::string config_dir;
Win *win;

Window current = 0, current_app = 0;
Trace *trace = 0;
Trace::Point orig;
bool in_proximity = false;
boost::shared_ptr<sigc::slot<void, RStroke> > stroke_action;
Grabber::XiDevice *current_dev = 0;
std::set<guint> xinput_pressed;
bool dead = false;

class Handler;
Handler *handler = 0;
ActionDBWatcher *action_watcher = 0;
boost::shared_ptr<sigc::slot<void, std::string> > window_selected;


Trace *trace_composite() {
	try {
		return new Composite();
	} catch (std::exception &e) {
		if (verbosity >= 1)
			printf(_("Falling back to Shape method: %s\n"), e.what());
		return new Shape();
	}
}

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
				return trace_composite();
		}
	} catch (DBusException &e) {
		printf(_("Error: %s\n"), e.what());
		return trace_composite();
	}

}

void replay(Time t) { XAllowEvents(dpy, ReplayPointer, t); }
void discard(Time t) { XAllowEvents(dpy, AsyncPointer, t); }

static void fake_click(guint b) {
	XTestFakeButtonEvent(dpy, b, True, CurrentTime);
	XTestFakeButtonEvent(dpy, b, False, CurrentTime);
}

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
	virtual void motion(RTriple e) {}
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
	void remap(const std::map<guint, guint> &map);
	virtual void init() {}
	virtual bool idle() { return false; }
	virtual ~Handler() {
		if (child)
			delete child;
	}
	virtual std::string name() = 0;
	virtual Grabber::State grab_mode() = 0;
};


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
		int screen = DefaultScreen(dpy);
		int left = DisplayWidth(dpy, screen) - 10;
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
	virtual void press_core(guint b, Time t, bool xi) {
		replay(t);
		if (!in_proximity)
			proximity_out();
	}
	virtual void proximity_out() {
		parent->replace_child(0);
	}
	virtual std::string name() { return "Ignore"; }
	virtual Grabber::State grab_mode() { return Grabber::ALL_SYNC; }
};

void bail_out();

static void reset_buttons(bool force);

RAction handle_stroke(RStroke s, RTriple e) {
	Ranking *ranking = new Ranking;
	RAction act = actions.get_action_list(grabber->get_wm_class())->handle(s, *ranking);
	if (act)
		act->prepare();
	ranking->x = (int)e->x;
	ranking->y = (int)e->y;
	if (!IS_CLICK(act) || prefs.show_clicks.get())
		Glib::signal_idle().connect(sigc::mem_fun(ranking, &Ranking::show));
	else
		delete ranking;
	return act;
}

void bail_out() {
	handler->replace_child(0);
	grabber->release_all();
	discard(CurrentTime);
	xinput_pressed.clear();
	reset_buttons(true);
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
			printf(_("Error: %s failed.  Is easystroke already running?\n"),
					e->request_code == X_GrabButton ? _("A grab") : _("An XInput grab"));
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

int xIOErrorHandler(Display *dpy2) {
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

class WaitForButtonHandler : public Handler, protected Timeout {
	guint button;
	bool down;
public:
	WaitForButtonHandler(guint b, bool d) : button(b), down(d) {
		set_timeout(100);
	}
	virtual void timeout() {
		printf(_("Warning: WaitForButtonHandler timed out\n"));
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
	virtual Grabber::State grab_mode() { return parent->grab_mode(); }
};


std::map<guint, guint> pointer_map;

void remap_grabs(const std::map<guint, guint> &map) {
	if (verbosity >= 4) {
		printf("Old:\n");
		for (std::map<guint, guint>::const_iterator i = pointer_map.begin(); i != pointer_map.end(); i++)
			printf("%d -> %d\n", i->first, i->second);
		printf("New:\n");
		for (std::map<guint, guint>::const_iterator i = map.begin(); i != map.end(); i++)
			printf("%d -> %d\n", i->first, i->second);
	}

	std::map<guint, guint>::const_iterator j1 = pointer_map.begin(), j2 = map.begin();
	std::set<guint> to_press, to_release, to_grab, to_ungrab;
	while (true) {
		if (j1 == pointer_map.end() && j2 == map.end())
			break;
		if (j2 == map.end() || (j1 != pointer_map.end() && j1->first < j2->first)) {
			if (xinput_pressed.count(j1->first)) {
				if (j1->second)
					to_release.insert(j1->second);
				to_press.insert(j1->first);
			}
			to_ungrab.insert(j1->first);
			++j1;
			continue;
		}
		if (j1 == pointer_map.end() || j2->first < j1->first) {
			if (xinput_pressed.count(j2->first)) {
				to_release.insert(j2->first);
				if (j2->second)
					to_press.insert(j2->second);
			}
			to_grab.insert(j2->first);
			++j2;
			continue;
		}
		if (j1->second != j2->second) {
			if (j1->second)
				to_release.insert(j1->second);
			if (j2->second)
				to_press.insert(j2->second);
		}
		++j1; ++j2;
	}
	pointer_map = map;
	for (std::set<guint>::iterator i = to_release.begin(); i != to_release.end(); ++i) {
		if (verbosity >= 3)
			printf("fake release: %d\n", *i);
		XTestFakeButtonEvent(dpy, *i, False, CurrentTime);
	}
	for (std::set<guint>::iterator i = to_ungrab.begin(); i != to_ungrab.end(); ++i) {
		if (verbosity >= 3)
			printf("ungrab: %d\n", *i);
		XUngrabButton(dpy, *i, AnyModifier, ROOT);
	}
	for (std::set<guint>::iterator i = to_press.begin(); i != to_press.end(); ++i) {
		if (verbosity >= 3)
			printf("fake press: %d\n", *i);
		XTestFakeButtonEvent(dpy, *i, True, CurrentTime);
	}
	for (std::set<guint>::iterator i = to_grab.begin(); i != to_grab.end(); ++i) {
		if (verbosity >= 3)
			printf("grab: %d\n", *i);
		XGrabButton(dpy, *i, AnyModifier, ROOT, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
	}
}

static int mapping_events = 0;

// This can potentially mess up the user's mouse, make sure that it doesn't fail
static void remap_pointer() {
	int n = XGetPointerMapping(dpy, 0, 0);
	unsigned char m[n];
	for (int i = 0; i<n; i++)
		m[i] = i+1;
	for (std::map<guint, guint>::const_iterator i = pointer_map.begin(); i != pointer_map.end(); ++i)
		m[i->first - 1] = i->second;
	while (MappingBusy == XSetPointerMapping(dpy, m, n)) {
		printf("Warning: remapping buttons failed, retrying...\n");
		grabber->release_all(n);
		XSync(dpy, False);
		usleep(50);
	}
	mapping_events++;
}

static void reset_buttons(bool force) {
	if (!pointer_map.size() && !force)
		return;
	if (xi_15) {
		pointer_map.clear();
		remap_pointer();
	} else {
		std::map<guint, guint> map;
		remap_grabs(map);
	}

}

void Handler::remap(const std::map<guint, guint> &map) {
	if (!xi_15) {
		remap_grabs(map);
		return;
	}
	std::set<guint> remap;
	std::map<guint, guint>::const_iterator j1 = pointer_map.begin(), j2 = map.begin();
	while (true) {
		if (j1 == pointer_map.end()) {
			for (; j2 != map.end(); ++j2)
				remap.insert(j2->first);
			break;
		}
		if (j2 == map.end()) {
			for (; j1 != pointer_map.end(); ++j1)
				remap.insert(j1->first);
			break;
		}
		guint b1 = j1->first, b2 = j2->first;
#ifndef SERVER16_BUTTON_MAPPING_PATCH
		if (b2 != j2->second)
			remap.insert(b2);
#endif
		if (b1 < b2) {
			remap.insert(b1);
			++j1;
			continue;
		}
		if (b2 < b1) {
			remap.insert(b2);
			++j2;
			continue;
		}
		if (j1->second != j2->second)
			remap.insert(b1);
		++j1; ++j2;
	}
	if (!remap.size())
		return;

	pointer_map = map;

	guint last = 0;
	if (current_dev)
		for (std::set<guint>::iterator i = remap.begin(); i != remap.end(); ++i)
			if (xinput_pressed.count(*i)) {
				last = *i;
				current_dev->fake_button(last, false);
			}

	remap_pointer();
#ifndef SERVER16_BUTTON_MAPPING_PATCH
	if (!current_dev)
		return;
	XDevice *dev = current_dev->dev;
	{
		int n = XGetDeviceButtonMapping(dpy, dev, 0, 0);
		unsigned char m[n];
		for (int i = 0; i<n; i++)
			m[i] = i+1;
		while (MappingBusy == XSetDeviceButtonMapping(dpy, dev, m, n)) {
			printf("Warning: remapping device buttons failed, retrying...\n");
			for (int i = 1; i<=n; i++)
				current_dev->fake_button(i, false);
			XSync(dpy, False);
			usleep(50);
		}
	}
#endif

	if (last) {
		for (std::set<guint>::iterator i = remap.begin(); i != remap.end(); ++i)
			if (xinput_pressed.count(*i)) {
				current_dev->fake_button(*i, true);
			}
		replace_child(new WaitForButtonHandler(last, true));
	}
}

inline float abs(float x) { return x > 0 ? x : -x; }

class AbstractScrollHandler : public Handler {
	float last_x, last_y;
	Time last_t;
	float offset_x, offset_y;
	Glib::ustring str;

protected:
	AbstractScrollHandler() : last_t(0), offset_x(0.0), offset_y(0.0) {}
	virtual void fake_wheel(int b1, int n1, int b2, int n2) {
		for (int i = 0; i<n1; i++)
			fake_click(b1);
		for (int i = 0; i<n2; i++)
			fake_click(b2);
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
		offset_x += curve((e->x-last_x)/(e->t-last_t))*(e->t-last_t)/10.0;
		offset_y += curve((e->y-last_y)/(e->t-last_t))*(e->t-last_t)/5.0;
		last_x = e->x;
		last_y = e->y;
		last_t = e->t;
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
public:
	ScrollHandler(RModifiers mods_) : mods(mods_) {
		// If you want to use buttons > 9, you're on your own..
		map[1] = 0; map[2] = 0; map[3] = 0; map[8] = 0; map[9] = 0;
	}
	virtual void init() {
		remap(map);
	}
	virtual void motion(RTriple e) {
		if (xinput_pressed.size())
			AbstractScrollHandler::motion(e);
	}
	virtual void press_core(guint b, Time t, bool xi) {
		if (!xi_15)
			XTestFakeButtonEvent(dpy, b, False, CurrentTime);
	}
	virtual void release(guint b, RTriple e) {
		if (!in_proximity && !xinput_pressed.size())
			parent->replace_child(0);
	}
	virtual void proximity_out() {
		parent->replace_child(0);
	}
	virtual ~ScrollHandler() {
		reset_buttons(false);
	}
	virtual std::string name() { return "Scroll"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

class ScrollAdvancedHandler : public AbstractScrollHandler {
	const std::map<guint, guint> &map;
public:
	ScrollAdvancedHandler(const std::map<guint, guint> &map_) : map(map_) {}
	virtual void init() {
		std::map<guint, guint> new_map = map;
		new_map.erase(4);
		new_map.erase(5);
		new_map.erase(6);
		new_map.erase(7);
		remap(new_map);
	}
	virtual void release(guint b, RTriple e) {
		if (b >= 4 && b <= 7)
			return;
		Handler *p = parent;
		p->replace_child(NULL);
		p->release(b, e);
	}
	virtual void press_core(guint b, Time t, bool xi) {
		if (!xi_15)
			XTestFakeButtonEvent(dpy, b, False, CurrentTime);
	}
	virtual void press(guint b, RTriple e) {
		if (b >= 4 && b <= 7)
			return;
		Handler *p = parent;
		p->replace_child(NULL);
		p->press(b, e);
	}
	virtual std::string name() { return "ScrollAdvanced"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

class AdvancedStrokeActionHandler : public Handler {
	RStroke s;
public:
	AdvancedStrokeActionHandler(RStroke s_, RTriple e) : s(s_) { discard(e->t); }
	virtual void press(guint b, RTriple e) {
		s->button = b;
		(*stroke_action)(s);
		parent->replace_child(NULL);
	}
	virtual void release(guint b, RTriple e) {
		(*stroke_action)(s);
		parent->replace_child(NULL);
	}
	virtual std::string name() { return "InstantStrokeAction"; }
	virtual Grabber::State grab_mode() { return Grabber::NONE; }
};

class AdvancedHandler : public Handler {
	RTriple e;
	guint remap_from;
	Time click_time;
	guint replay_button;
	std::map<int, RAction> as;
	std::map<int, Ranking *> rs;
	std::map<int, RModifiers> mods;
	RModifiers sticky_mods;

	guint button1, button2;

	std::map<guint, guint> map;

	void show_ranking(guint b, RTriple e) {
		if (!rs.count(b))
			return;
		Ranking *r = rs[b];
		if (r) {
			r->x = (int)e->x;
			r->y = (int)e->y;
			Glib::signal_idle().connect(sigc::mem_fun(r, &Ranking::show));
		}
		rs.erase(b);
	}
	AdvancedHandler(RTriple e_, std::map<int, RAction> &as_, std::map<int, Ranking *> rs_, guint b1, guint b2) :
		e(e_), remap_from(0), click_time(0), as(as_), rs(rs_), button1(b1), button2(b2) {
			for (std::map<int, RAction>::iterator i = as.begin(); i != as.end(); ++i)
				// TODO: Figure this out
				if (i->first)
					map[i->first] = 0;
				else
					map[b1] = 0;
		}
public:
	static Handler *create(RStroke s, RTriple e, guint b1, guint b2, Time press_t) {
		if (stroke_action)
			return new AdvancedStrokeActionHandler(s, e);

		std::map<int, RAction> as;
		std::map<int, Ranking *> rs;
		actions.get_action_list(grabber->get_wm_class())->handle_advanced(s, as, rs, b1, b2);
		if (as.count(b2) && IS_CLICK(as[b2])) {
			replay(press_t);
			return NULL;
		}
		if (as.count(b2))
			discard(press_t);
		else
			replay(press_t);
		return new AdvancedHandler(e, as, rs, b1, b2);
	}
	virtual void init() {
		press(button2 ? button2 : button1, e);
	}
	virtual void press_core(guint b, Time t, bool xi) {
		if (!xi_15)
			XTestFakeButtonEvent(dpy, b, False, CurrentTime);
	}
	virtual void press(guint b, RTriple e) {
		click_time = 0;
		int bb = (b == button1) ? button2 : b;
		show_ranking(bb, e);
		if (!as.count(bb)) {
			sticky_mods.reset();
			remap(map);
			return;
		}
		RAction act = as[bb];
		if (IS_SCROLL(act)) {
			click_time = e->t;
			replay_button = b;
			mods[b] = act->prepare();
			sticky_mods.reset();
			replace_child(new ScrollAdvancedHandler(map));
			return;
		}
		if (IS_IGNORE(act)) {
			click_time = e->t;
			replay_button = b;
		}
		IF_BUTTON(act, b2) {
			if (remap_from)
				mods[remap_from].reset();
			std::map<guint, guint> new_map = map;
			remap_from = b;
			new_map[b2] = 0;
			new_map[remap_from] = b2;
			mods[remap_from] = act->prepare();
			sticky_mods.reset();
			remap(new_map);
			return;
		}
		remap(map);
		mods[b] = act->prepare();
		if (IS_KEY(act))
			sticky_mods = mods[b];
		else
			sticky_mods.reset();
		act->run();
	}
	virtual void release(guint b, RTriple e) {
		if (b == remap_from && !xi_15 && pointer_map[b])
			XTestFakeButtonEvent(dpy, pointer_map[b], False, CurrentTime);
		if (xinput_pressed.size() == 0) {
			reset_buttons(false);
			Handler *h = NULL;
			if (e->t < click_time + 250 && b == replay_button) {
				sticky_mods.reset();
				mods.clear();
				if (xi_15) {
					current_dev->fake_button(b, true);
					current_dev->fake_button(b, false);
					h = new WaitForButtonHandler(b, false);
				} else {
					fake_click(b);
				}
			}
			parent->replace_child(h);
			return;
		}
		if (b == remap_from)
			remap_from = 0;
		remap(map);
		mods.erase(b);
	}
	virtual ~AdvancedHandler() {
		for (std::map<int, Ranking *>::iterator i = rs.begin(); i != rs.end(); i++)
			delete i->second;
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
		if (!is_gesture || grabber->is_instant(button))
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
			parent->replace_child(NULL);
			XTestFakeRelativeMotionEvent(dpy, 0, 0, 5);
			return;
		}
		if (!is_gesture)
			cur->clear();
		RStroke s = Stroke::create(*cur, button, 0, true);
		parent->replace_child(AdvancedHandler::create(s, last, button, 0, press_t));
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
			replay(t);
		// At this point we already have an xi press, so we are
		// guarenteed to either get another press or a release.
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
			replay(0);
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

		if (grabber->xinput) {
			parent->replace_child(AdvancedHandler::create(s, e, button, b, press_t));
		} else {
			printf(_("Error: You need XInput to use advanced gestures\n"));
			parent->replace_child(NULL);
		}
	}

	virtual void release(guint b, RTriple e) {
		if (calc_speed(e))
			return;
		RStroke s = finish(0);

		if (stroke_action) {
			discard(press_t);
			(*stroke_action)(s);
			parent->replace_child(0);
			return;
		}
		RAction act = handle_stroke(s, e);
		win->show_success(act);
		RModifiers mods;
		if (act) {
			mods = act->prepare();
			act->run();
		}
		else
			XBell(dpy, 0);
		if (IS_CLICK(act)) {
			if (grabber->xinput)
				replay(press_t);
			else
				act = Button::create((Gdk::ModifierType)0, b);
		} else {
			if (grabber->xinput)
				discard(press_t);
		}
		if (IS_IGNORE(act)) {
			parent->replace_child(new IgnoreHandler(mods));
			return;
		}
		if (IS_SCROLL(act) && grabber->xinput) {
			parent->replace_child(new ScrollHandler(mods));
			return;
		}
		IF_BUTTON(act, press)
			if (press && !(!repeated && xinput_pressed.count(b) && press == button)) {
				grabber->suspend();
				XTestFakeButtonEvent(dpy, press, True, CurrentTime);
				XTestFakeButtonEvent(dpy, press, False, CurrentTime);
				grabber->resume();
			}
		parent->replace_child(0);
	}
public:
	StrokeHandler(guint b, RTriple e) : button(b), is_gesture(false), drawing(false), last(e),
	repeated(false), min_speed(0.001*prefs.min_speed.get()), speed(min_speed * exp(-k*prefs.init_timeout.get())),
	use_timeout(prefs.init_timeout.get() && prefs.min_speed.get()), press_t(e->t) {
		orig.x = e->x; orig.y = e->y;
		cur = PreStroke::create();
		cur->add(e);
		if (grabber->xinput)
			set_timeout(prefs.init_timeout.get());
		else
			discard(press_t);
	}
	~StrokeHandler() {
		trace->end();
	}
	virtual std::string name() { return "Stroke"; }
	virtual Grabber::State grab_mode() { return Grabber::BUTTON; }
};

float StrokeHandler::k = -0.01;

class IdleHandler : public Handler, Timeout {
protected:
	virtual void init() {
		reset_buttons(true);
	}
	virtual void press_core(guint b, Time t, bool xi) {
		if (xi)
			return;
		if (b != 1 || !grabber->get_timing_workaround()) {
			replay(t);
			return;
		}
		Grabber::XiDevice *dev;
		unsigned int state = grabber->get_device_button_state(dev);
		if (state & (state-1)) {
			discard(t);
			if (verbosity >= 2)
				printf("Using wacom workaround\n");
			for (int i = 1; i < 32; i++)
				if (state & (1 << i))
					dev->fake_button(i, false);
			for (int i = 31; i; i--)
				if (state & (1 << i))
					dev->fake_button(i, true);
			// This is probably not necessary, but we need to be on the safe
			// side.  If anything goes wrong, the timeout fires and we discard everything
			set_timeout(250);
		} else {
			replay(t);
		}
	}
	virtual void timeout() { discard(CurrentTime); }
	virtual void press(guint b, RTriple e) {
		if (grabber->is_instant(b)) {
			remove_timeout();
			PreStroke ps;
			RStroke s = Stroke::create(ps, b, b, false);
			replace_child(AdvancedHandler::create(s, e, b, b, e->t));
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
		win->get_window().get_window()->lower();
		set_timeout(100);
	}
	virtual std::string name() { return "Select"; }
	virtual Grabber::State grab_mode() { return Grabber::ALL_SYNC; }
};


void run_by_name(const char *str) {
	for (ActionDB::const_iterator i = actions.begin(); i != actions.end(); i++) {
		if (i->second.name == std::string(str)) {
			RModifiers mods = i->second.action->prepare();
			i->second.action->run();
			return;
		}
	}
	printf(_("Warning: No action \"%s\" defined\n"), str);
}

void quit(int) {
	if (dead)
		bail_out();
	if (handler->top()->idle() || dead)
		Gtk::Main::quit();
	else
		dead = true;
}

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
	~Main();
};

class ReloadTrace : public Timeout {
	void timeout() {
		if (verbosity >= 2)
			printf("Reloading gesture display\n");
		Trace *new_trace = init_trace();
		delete trace;
		trace = new_trace;
	}
} reload_trace;

void schedule_reload_trace() { reload_trace.set_timeout(1000); }

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
		printf(_("Error initializing D-BUS\n"));
		exit(EXIT_FAILURE);
	}
	DBusGProxy *proxy = dbus_g_proxy_new_for_name(bus, "org.easystroke", "/org/easystroke", "org.easystroke");
	dbus_g_proxy_call_no_reply(proxy, "send", G_TYPE_STRING, str, G_TYPE_INVALID);
}

bool start_dbus();

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
		send_dbus(argv[2]);
		exit(EXIT_SUCCESS);
	}

	display = parse_args_and_init_gtk();
	create_config_dir();
	unsetenv("DESKTOP_AUTOSTART_ID");

	signal(SIGINT, &quit);
	signal(SIGCHLD, SIG_IGN);

	Gtk::LinkButton::set_uri_hook(sigc::ptr_fun(&open_uri));

	dpy = XOpenDisplay(display.c_str());
	if (!dpy) {
		printf(_("Couldn't open display.\n"));
		exit(EXIT_FAILURE);
	}

	prefs.init();
	action_watcher = new ActionDBWatcher;
	action_watcher->init();

	grabber = new Grabber;
	grabber->grab(Grabber::BUTTON);

	trace = init_trace();
	Glib::RefPtr<Gdk::Screen> screen = Gdk::Display::get_default()->get_default_screen();
	g_signal_connect(screen->gobj(), "composited-changed", &schedule_reload_trace, NULL);
	screen->signal_size_changed().connect(sigc::ptr_fun(&schedule_reload_trace));
	Notifier *trace_notify = new Notifier(sigc::ptr_fun(&schedule_reload_trace));
	prefs.trace.connect(trace_notify);
	prefs.color.connect(trace_notify);

	handler = new IdleHandler;
	handler->init();
	XTestGrabControl(dpy, True);

	start_dbus();
}

void Main::run() {
	Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(dpy), Glib::IO_IN);
	io->connect(sigc::mem_fun(*this, &Main::handle));
	io->attach();
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

std::string Main::parse_args_and_init_gtk() {
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
	if (config_dir == "") {
		config_dir = getenv("HOME");
		config_dir += "/.easystroke";
	}
	struct stat st;
	if (lstat(config_dir.c_str(), &st) == -1) {
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
	config_dir += "/";
}

extern Window get_app_window(Window &w);

void Grabber::XiDevice::translate_coords(int *axis_data, float &x, float &y) {
	if (!absolute) {
		valuators[0] += axis_data[0];
		valuators[1] += axis_data[1];
		x = valuators[0];
		y = valuators[1];
		return;
	}
	valuators[0] = axis_data[0];
	valuators[1] = axis_data[1];
	int w = DisplayWidth(dpy, DefaultScreen(dpy)) - 1;
	int h = DisplayHeight(dpy, DefaultScreen(dpy)) - 1;
	if (!rotated) {
		x = rescaleValuatorAxis(axis_data[0], min_x, max_x, w);
		y = rescaleValuatorAxis(axis_data[1], min_y, max_y, h);
	} else {
		x = rescaleValuatorAxis(axis_data[0], min_y, max_y, w);
		y = rescaleValuatorAxis(axis_data[1], min_x, max_x, h);
	}
}

bool Grabber::XiDevice::translate_known_coords(int sx, int sy, int *axis_data, float &x, float &y) {
	sx += offset_x;
	sy += offset_y;
	if (!absolute) {
		valuators[0] = sx;
		valuators[1] = sy;
		x = valuators[0];
		y = valuators[1];
		return true;
	}
	valuators[0] = axis_data[0];
	valuators[1] = axis_data[1];
	int w = DisplayWidth(dpy, DefaultScreen(dpy)) - 1;
	int h = DisplayHeight(dpy, DefaultScreen(dpy)) - 1;
	x        = rescaleValuatorAxis(axis_data[0], min_x, max_x, w);
	y        = rescaleValuatorAxis(axis_data[1], min_y, max_y, h);
	if (axis_data[0] == sx && axis_data[1] == sy)
		return true;
	float x2 = rescaleValuatorAxis(axis_data[0], min_y, max_y, w);
	float y2 = rescaleValuatorAxis(axis_data[1], min_x, max_x, h);
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
		win->get_window().raise();
	}
}

class PresenceWatcher : public Timeout {
	virtual void timeout() {
		grabber->update_device_list();
		win->prefs_tab->update_device_list();
	}
} presence_watcher;

struct ButtonTime {
	guint button;
	Time time;
};

Bool is_device_press(Display *dpy_, XEvent *ev, XPointer arg) {
	ButtonTime *bt = (ButtonTime *)arg;
	XDeviceButtonEvent* bev = (XDeviceButtonEvent *)ev;
	return grabber->is_event(ev->type, Grabber::DOWN) && bt->button == bev->button && bt->time == bev->time;
}

void Main::handle_event(XEvent &ev) {
	static guint last_button = 0;
	static Time last_time = 0;
#define H (handler->top())

	switch(ev.type) {
	case EnterNotify:
	case LeaveNotify:
		handle_enter_leave(ev);
		return;

	case PropertyNotify:
		static XAtom WM_CLASS("WM_CLASS");
		if (current && ev.xproperty.window == current && ev.xproperty.atom == *WM_CLASS)
			grabber->update(current);
		return;

	case MotionNotify:
		if (verbosity >= 4)
			printf("Motion: (%d, %d)\n", ev.xmotion.x, ev.xmotion.y);
		if (!grabber->xinput)
			H->motion(create_triple(ev.xmotion.x, ev.xmotion.y, ev.xmotion.time));
		return;

	case ButtonPress:
		if (verbosity >= 3)
			printf("Press: %d (%d, %d) at t = %ld\n", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y, ev.xbutton.time);
		if (!grabber->xinput)
			H->press(ev.xbutton.button, create_triple(ev.xbutton.x, ev.xbutton.y, ev.xbutton.time));
		else {
			bool xi = ev.xbutton.button == last_button && ev.xbutton.time == last_time;
			if (!xi) {
				XEvent xi_ev;
				ButtonTime bt = { ev.xbutton.button, ev.xbutton.time };
				if (XCheckIfEvent(dpy, &xi_ev, &is_device_press, (XPointer)&bt)) {
					handle_event(xi_ev);
					xi = true;
				}
			}
			H->press_core(ev.xbutton.button, ev.xbutton.time, xi);

		}
		return;

	case ButtonRelease:
		if (verbosity >= 3)
			printf("Release: %d (%d, %d)\n", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
		if (!grabber->xinput)
			H->release(ev.xbutton.button, create_triple(ev.xbutton.x, ev.xbutton.y, ev.xbutton.time));
		return;

	case MappingNotify:
		if (!xi_15 || ev.xmapping.request != MappingPointer)
			return;
		if (mapping_events) {
			mapping_events--;
			return;
		}
		remap_pointer();
		return;
	}

	if (grabber->is_event(ev.type, Grabber::DOWN)) {
		XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
		if (verbosity >= 3)
			printf("Press (Xi): %d (%d, %d, %d, %d, %d) at t = %ld\n",bev->button, bev->x, bev->y,
					bev->axis_data[0], bev->axis_data[1], bev->axis_data[2], bev->time);
		last_button = bev->button;
		last_time = bev->time;
		if (xinput_pressed.size()) {
			if (!current_dev || current_dev->dev->device_id != bev->deviceid)
				return;
		} else
			current_dev = grabber->get_xi_dev(bev->deviceid);
		if (!current_dev)
			return;
		xinput_pressed.insert(bev->button);
		float x, y;
		if (xi_15 && xinput_pressed.size() > 1)
			current_dev->translate_coords(bev->axis_data, x, y);
		else
			current_dev->translate_known_coords(bev->x, bev->y, bev->axis_data, x, y);
		H->press(bev->button, create_triple(x, y, bev->time));
		return;
	}

	if (grabber->is_event(ev.type, Grabber::UP)) {
		XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
		if (verbosity >= 3)
			printf("Release (Xi): %d (%d, %d, %d, %d, %d)\n", bev->button, bev->x, bev->y,
					bev->axis_data[0], bev->axis_data[1], bev->axis_data[2]);
		if (!current_dev || current_dev->dev->device_id != bev->deviceid)
			return;
		xinput_pressed.erase(bev->button);
		float x, y;
		if (xi_15)
			current_dev->translate_coords(bev->axis_data, x, y);
		else
			current_dev->translate_known_coords(bev->x, bev->y, bev->axis_data, x, y);

		H->release(bev->button, create_triple(x, y, bev->time));
		return;
	}

	if (grabber->is_event(ev.type, Grabber::MOTION)) {
		XDeviceMotionEvent* mev = (XDeviceMotionEvent *)&ev;
		if (verbosity >= 4)
			printf("Motion (Xi): (%d, %d, %d, %d, %d)\n", mev->x, mev->y,
					mev->axis_data[0], mev->axis_data[1], mev->axis_data[2]);
		if (!current_dev || current_dev->dev->device_id != mev->deviceid)
			return;
		float x, y;
		if (xi_15)
			current_dev->translate_coords(mev->axis_data, x, y);
		else
			current_dev->translate_known_coords(mev->x, mev->y, mev->axis_data, x, y);
		int z = 0;
		if (current_dev->supports_pressure)
			z = current_dev->normalize_pressure(mev->axis_data[2]);
		if (prefs.pressure_abort.get() && z >= prefs.pressure_threshold.get())
			H->pressure();
		H->motion(create_triple(x, y, mev->time));
		return;
	}

	if (grabber->proximity_selected && grabber->is_event(ev.type, Grabber::PROX_IN)) {
		in_proximity = true;
		if (verbosity >= 3)
			printf("Proximity: In\n");
		return;
	}
	if (grabber->proximity_selected && grabber->is_event(ev.type, Grabber::PROX_OUT)) {
		in_proximity = false;
		if (verbosity >= 3)
			printf("Proximity: Out\n");
		H->proximity_out();
		return;
	}
	if (ev.type == grabber->event_presence) {
		if (verbosity >= 2)
			printf("Device Presence\n");
		presence_watcher.set_timeout(2000);
		return;
	}
	if (ev.type == grabber->mapping_notify) {
		XDeviceMappingEvent* dev = (XDeviceMappingEvent *)&ev;
		if (!xi_15 || dev->request != MappingPointer)
			return;
		Grabber::XiDevice *xi_dev = grabber->get_xi_dev(dev->deviceid);
		if (xi_dev)
			xi_dev->update_pointer_mapping();
		return;
	}
#undef H
}

bool Main::handle(Glib::IOCondition) {
	while (XPending(dpy)) {
		try {
			XEvent ev;
			XNextEvent(dpy, &ev);
			if (!grabber->handle(ev))
				handle_event(ev);
		} catch (GrabFailedException) {
			printf(_("Error: A grab failed.  Resetting...\n"));
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

int main(int argc_, char **argv_) {
	argc = argc_;
	argv = argv_;
	Main mn;
	mn.run();
	if (verbosity >= 2)
		printf("Exiting...\n");
	return EXIT_SUCCESS;
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

void SendKey::run() {
	XTestFakeKeyEvent(dpy, code, true, 0);
	XTestFakeKeyEvent(dpy, code, false, 0);
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
		set_timeout(150);
		all.insert(this);
		update_mods();
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

void Misc::run() {
	switch (type) {
		case SHOWHIDE:
			win->show_hide();
			return;
		case UNMINIMIZE:
			grabber->unminimize();
			return;
		case DISABLE:
			win->toggle_disabled();
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
