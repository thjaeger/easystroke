#include "handler.h"
#include "log.h"
#include "globals.h"
#include "trace.h"
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/XKBlib.h>
#include <X11/Xproto.h>
#include "actions/modAction.h"
#include <sstream>
#include <iomanip>
#include <utility>

XState *xstate = nullptr;

extern Window get_app_window(Window w);
extern Source<Window> current_app_window;

std::shared_ptr<sigc::slot<void, std::shared_ptr<Gesture>> > stroke_action;

bool XState::idle() {
	return !handler->child;
}

void XState::queue(sigc::slot<void> f) {
	if (idle()) {
        f();
        context->xServer->flush();
    } else {
        queued.push_back(f);
    }
}

void XState::handle_enter_leave(XEvent &ev) {
	if (ev.xcrossing.mode == NotifyGrab)
		return;
	if (ev.xcrossing.detail == NotifyInferior)
		return;
	Window w = ev.xcrossing.window;
	if (ev.type == EnterNotify) {
		current_app_window.set(get_app_window(w));
		g_debug("Entered window 0x%lx -> 0x%lx", w, current_app_window.get());
	} else {
		g_warning("Error: Bogus Enter/Leave event");
	};
}

#define H (handler->top())
void XState::handle_event(XEvent &ev) {

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
            g_debug("Press (master): %d (%d, %d) at t = %ld", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y,
                    ev.xbutton.time);
            H->press_master(ev.xbutton.button, ev.xbutton.time);
            return;

        case ClientMessage:
            if (ev.xclient.window != ping_window)
                return;
            return;

        case MappingNotify:
            if (ev.xmapping.request == MappingPointer)
                update_core_mapping();
            if (ev.xmapping.request == MappingKeyboard || ev.xmapping.request == MappingModifier)
                XRefreshKeyboardMapping(&ev.xmapping);
            return;
        case GenericEvent:
            if (ev.xcookie.extension == grabber->opcode && context->xServer->getEventData(&ev.xcookie)) {
                handle_xi2_event((XIDeviceEvent *) ev.xcookie.data);
                context->xServer->freeEventData(&ev.xcookie);
            }
    }
}

void XState::activate_window(Window w, Time t) {
    if (w == get_window(context->xServer->ROOT, context->xServer->atoms.NET_ACTIVE_WINDOW))
        return;

    Atom window_type = get_atom(w, context->xServer->atoms.NET_WM_WINDOW_TYPE);
    if (window_type == context->xServer->atoms.NET_WM_WINDOW_TYPE_DOCK)
        return;

    auto *wm_hints = context->xServer->getWMHints(w);
    if (wm_hints) {
        bool input = wm_hints->input;
        XServerProxy::free(wm_hints);
        if (!input)
            return;
    }

    if (!has_atom(w, context->xServer->atoms.WM_PROTOCOLS, context->xServer->atoms.WM_TAKE_FOCUS))
        return;

    XWindowAttributes attr;
    if (context->xServer->getWindowAttributes(w, &attr) && attr.override_redirect) {
        return;
    }

    g_debug("Giving focus to window 0x%lx", w);

    icccm_client_message(w, context->xServer->atoms.WM_TAKE_FOCUS, t);
}

Window XState::get_window(Window w, Atom prop) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_return = nullptr;

    if (context->xServer->getWindowProperty(w, prop, 0, sizeof(Atom), False, XA_WINDOW, &actual_type, &actual_format,
                                         &nitems, &bytes_after, &prop_return) != Success)
        return None;
    if (!prop_return)
        return None;
    Window ret = *(Window *) prop_return;
    XServerProxy::free(prop_return);
    return ret;
}

Atom XState::get_atom(Window w, Atom prop) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_return = nullptr;

    if (context->xServer->getWindowProperty(w, prop, 0, sizeof(Atom), False, XA_ATOM, &actual_type, &actual_format,
                                         &nitems, &bytes_after, &prop_return) != Success)
        return None;
    if (!prop_return)
        return None;
    Atom atom = *(Atom *) prop_return;
    XServerProxy::free(prop_return);
    return atom;
}

bool XState::has_atom(Window w, Atom prop, Atom value) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_return = nullptr;

    if (context->xServer->getWindowProperty(w, prop, 0, sizeof(Atom), False, XA_ATOM, &actual_type, &actual_format,
                                         &nitems, &bytes_after, &prop_return) != Success)
        return None;
    if (!prop_return)
        return None;
    Atom *atoms = (Atom *) prop_return;
    bool ans = false;
    for (unsigned long i = 0; i < nitems; i++)
        if (atoms[i] == value)
            ans = true;
    XServerProxy::free(prop_return);
    return ans;
}

void XState::icccm_client_message(Window w, Atom a, Time t) {
    XClientMessageEvent ev;
    ev.type = ClientMessage;
    ev.window = w;
    ev.message_type = context->xServer->atoms.WM_PROTOCOLS;
    ev.format = 32;
    ev.data.l[0] = a;
    ev.data.l[1] = t;
    context->xServer->sendEvent(w, False, 0, (XEvent *) &ev);
}

static std::string render_coordinates(XIValuatorState *valuators, double *values) {
    std::ostringstream ss;
    ss << std::setprecision(3) << std::fixed;

    int n = 0;
    for (int i = valuators->mask_len - 1; i >= 0; i--) {
        if (XIMaskIsSet(valuators->mask, i)) {
            n = i + 1;
            break;
        }
    }
    bool first = true;
    int elt = 0;
    for (int i = 0; i < n; i++) {
        if (first)
            first = false;
        else
            ss << ", ";
        if (XIMaskIsSet(valuators->mask, i))
            ss << values[elt++];
        else
            ss <<"*";
    }

    return ss.str();
}

static double get_axis(XIValuatorState &valuators, int axis) {
	if (axis < 0 || !XIMaskIsSet(valuators.mask, axis))
		return 0.0;
	double *val = valuators.values;
	for (int i = 0; i < axis; i++)
		if (XIMaskIsSet(valuators.mask, i))
			val++;
	return *val;
}

void XState::report_xi2_event(XIDeviceEvent *event, const char *type) {
    if (event->detail) {
        g_debug("%s (XI2): %d (%.3f, %.3f) - (%s) at t = %ld", type, event->detail, event->root_x, event->root_y,
               render_coordinates(&event->valuators, event->valuators.values).c_str(), event->time);
    } else {
        g_debug("%s (XI2): (%.3f, %.3f) - (%s) at t = %ld", type, event->root_x, event->root_y,
               render_coordinates(&event->valuators, event->valuators.values).c_str(), event->time);
    }
}

void XState::handle_xi2_event(XIDeviceEvent *event) {
	switch (event->evtype) {
		case XI_ButtonPress:
			if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG))
				report_xi2_event(event, "Press");
			if (!xinput_pressed.empty()) {
				if (!current_dev || current_dev->dev != event->deviceid)
					break;
			} else {
				current_app_window.set(get_app_window(event->child));
				g_debug("Active window 0x%lx -> 0x%lx", event->child, current_app_window.get());
			}
			current_dev = grabber->get_xi_dev(event->deviceid);
			if (!current_dev) {
				g_warning("Warning: Spurious device event");
				break;
			}
			if (current_dev->master)
                context->xServer->setClientPointer(None, current_dev->master);
			if (xinput_pressed.empty()) {
				guint default_mods = grabber->get_default_mods(event->detail);
				if (default_mods == AnyModifier || default_mods == (guint)event->mods.base)
					modifiers = AnyModifier;
				else
					modifiers = event->mods.base;
			}
			xinput_pressed.insert(event->detail);
			in_proximity = get_axis(event->valuators, current_dev->proximity_axis);
			H->press(event->detail, CursorPosition(event->root_x, event->root_y, event->time));
			break;
		case XI_ButtonRelease:
			if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG))
				report_xi2_event(event, "Release");
			if (!current_dev || current_dev->dev != event->deviceid)
				break;
			xinput_pressed.erase(event->detail);
			in_proximity = get_axis(event->valuators, current_dev->proximity_axis);
			H->release(event->detail, CursorPosition(event->root_x, event->root_y, event->time));
			break;
		case XI_Motion:
			if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG))
				report_xi2_event(event, "Motion");
			if (!current_dev || current_dev->dev != event->deviceid)
				break;
			H->motion(CursorPosition(event->root_x, event->root_y, event->time));
			break;
		case XI_RawMotion:
			in_proximity = get_axis(((XIRawEvent *)event)->valuators, current_dev->proximity_axis);
			handle_raw_motion((XIRawEvent *)event);
			break;
		case XI_HierarchyChanged:
			grabber->hierarchy_changed((XIHierarchyEvent *)event);
	}
}

void XState::handle_raw_motion(XIRawEvent *event) {
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

	if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG)) {
		g_debug("Raw motion (XI2): (%s) at t = %ld", render_coordinates(&event->valuators, event->raw_values).c_str(), event->time);
	}

	H->raw_motion(CursorPosition(x * current_dev->scale_x, y * current_dev->scale_y, event->time), abs_x, abs_y);
}

#undef H

bool XState::handle(Glib::IOCondition) {
    while (context->xServer->countPendingEvents()) {
        try {
            XEvent ev;
            context->xServer->nextEvent(&ev);
            if (!grabber->handle(ev))
                handle_event(ev);
        } catch (GrabFailedException &e) {
            g_error("%s", e.what());
        }
    }
    return true;
}

void XState::update_core_mapping() {
    unsigned char map[MAX_BUTTONS];
    int n = context->xServer->getPointerMapping(map, MAX_BUTTONS);
    core_inv_map.clear();
    for (int i = n - 1; i; i--)
        if (map[i] == i + 1)
            core_inv_map.erase(i + 1);
        else
            core_inv_map[map[i]] = i + 1;
}

void XState::fake_core_button(guint b, bool press) {
    if (core_inv_map.count(b))
        b = core_inv_map[b];
    context->xServer->fakeButtonEvent(b, press, CurrentTime);
}

void XState::fake_click(guint b) {
	fake_core_button(b, true);
	fake_core_button(b, false);
}

void Handler::replace_child(Handler *c) {
    delete child;

	child = c;
	if (child)
		child->parent = this;

	if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG)) {
		std::string stack;
		for (Handler *h = child ? child : this; h; h=h->parent) {
			stack = h->name() + " " + stack;
		}
		g_debug("New event handling stack: %s\n", stack.c_str());
	}
	Handler *new_handler = child ? child : this;
	grabber->grab(new_handler->grab_mode());
	if (child)
		child->init();
	while (!xstate->queued.empty() && xstate->idle()) {
		(*xstate->queued.begin())();
		xstate->queued.pop_front();
	}
}

class IgnoreHandler : public Handler {
    std::shared_ptr<Modifiers> mods;
	bool proximity;
public:
	IgnoreHandler(std::shared_ptr<Modifiers> mods_) : mods(std::move(mods_)), proximity(xstate->in_proximity && prefs.proximity) {}
	void press(guint b, CursorPosition e) override {
		if (xstate->current_dev->master) {
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), e.x, e.y, 0);
            context->xServer->fakeButtonEvent(b, true, CurrentTime);
        }
	}
	void motion(CursorPosition e) override {
        if (xstate->current_dev->master)
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), e.x, e.y, 0);
        if (proximity && !xstate->in_proximity)
            parent->replace_child(nullptr);
    }
	void release(guint b, CursorPosition e) override {
		if (xstate->current_dev->master) {
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), e.x, e.y, 0);
            context->xServer->fakeButtonEvent(b, false, CurrentTime);
        }
		if (proximity ? !xstate->in_proximity : xstate->xinput_pressed.empty())
			parent->replace_child(nullptr);
	}
	std::string name() override { return "Ignore"; }
	Grabber::State grab_mode() override { return Grabber::NONE; }
};

class ButtonHandler : public Handler {
    std::shared_ptr<Modifiers> mods;
	guint button, real_button;
	bool proximity;
public:
	ButtonHandler(std::shared_ptr<Modifiers> mods_, guint button_) :
		mods(std::move(mods_)),
		button(button_),
		real_button(0),
		proximity(xstate->in_proximity && prefs.proximity)
	{}
	void press(guint b, CursorPosition e) override {
		if (xstate->current_dev->master) {
            if (!real_button)
                real_button = b;
            if (real_button == b)
                b = button;
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), e.x, e.y, 0);
            context->xServer->fakeButtonEvent(b, true, CurrentTime);
        }
	}
	void motion(CursorPosition e) override {
        if (xstate->current_dev->master)
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), e.x, e.y, 0);
        if (proximity && !xstate->in_proximity)
            parent->replace_child(nullptr);
    }
	void release(guint b, CursorPosition e) override {
		if (xstate->current_dev->master) {
            if (real_button == b)
                b = button;
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), e.x, e.y, 0);
            context->xServer->fakeButtonEvent(b, false, CurrentTime);
        }
		if (proximity ? !xstate->in_proximity : xstate->xinput_pressed.empty())
			parent->replace_child(nullptr);
	}
	std::string name() override { return "Button"; }
	Grabber::State grab_mode() override { return Grabber::NONE; }
};

int XState::xErrorHandler(Display *dpy2, XErrorEvent *e) {
	if (!context->xServer->isSameDisplayAs(dpy2))
		return xstate->oldHandler(dpy2, e);
	if (e->error_code == BadWindow) {
		switch (e->request_code) {
			case X_ChangeWindowAttributes:
			case X_GetProperty:
			case X_QueryTree:
				return 0;
		}
	}

	char text[64];
    context->xServer->getErrorText(e->error_code, text, sizeof text);
	char msg[16];
	snprintf(msg, sizeof msg, "%d", e->request_code);
	char def[128];
	if (e->request_code < 128)
		snprintf(def, sizeof def, "request_code=%d, minor_code=%d", e->request_code, e->minor_code);
	else
		snprintf(def, sizeof def, "extension=%s, request_code=%d", xstate->opcodes[e->request_code].c_str(), e->minor_code);
	char dbtext[128];
    context->xServer->getErrorDatabaseText("XRequest", msg, def, dbtext, sizeof dbtext);
	g_warning("XError: %s: %s", text, dbtext);

	return 0;
}

int XState::xIOErrorHandler(Display *dpy2) {
	if (!context->xServer->isSameDisplayAs(dpy2))
		return xstate->oldIOHandler(dpy2);
    g_error("Connection to X server lost");
}

void XState::remove_device(int deviceid) {
	if (current_dev && current_dev->dev == deviceid)
		current_dev = nullptr;
}

void XState::ungrab(int deviceid) {
	if (current_dev && current_dev->dev == deviceid)
		xinput_pressed.clear();
}

class AbstractScrollHandler : public Handler {
	bool have_x, have_y;
    double last_x, last_y;
	Time last_t;
    double offset_x, offset_y;
	Glib::ustring str;
	int orig_x, orig_y;

protected:
	AbstractScrollHandler() : have_x(false), have_y(false), last_x(0.0), last_y(0.0), last_t(0), offset_x(0.0), offset_y(0.0) {
        if (!prefs.move_back || (xstate->current_dev && xstate->current_dev->absolute))
            return;
        Window dummy1, dummy2;
        int dummy3, dummy4;
        unsigned int dummy5;
        context->xServer->queryPointer(context->xServer->ROOT, &dummy1, &dummy2, &orig_x, &orig_y, &dummy3, &dummy4, &dummy5);
    }
	virtual void fake_wheel(int b1, int n1, int b2, int n2) {
		for (int i = 0; i<n1; i++)
			xstate->fake_click(b1);
		for (int i = 0; i<n2; i++)
			xstate->fake_click(b2);
	}
	static float curve(float v) {
		return v * exp(log(abs(v))/3);
	}
protected:
	void move_back() const {
        if (!prefs.move_back || (xstate->current_dev && xstate->current_dev->absolute))
            return;
        context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), orig_x, orig_y, 0);
    }
public:
	virtual void raw_motion(CursorPosition e, bool abs_x, bool abs_y) {
		double dx = abs_x ? (have_x ? e.x - last_x : 0) : e.x;
        double dy = abs_y ? (have_y ? e.y - last_y : 0) : e.y;

		if (abs_x) {
			last_x = e.x;
			have_x = true;
		}

		if (abs_y) {
			last_y = e.y;
			have_y = true;
		}

		if (!last_t) {
			last_t = e.t;
			return;
		}

		if (e.t == last_t)
			return;

		int dt = e.t - last_t;
		last_t = e.t;

		double factor = (prefs.scroll_invert ? 1.0 : -1.0) * prefs.scroll_speed;
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
    std::shared_ptr<Modifiers> mods;
	bool proximity;
public:
	explicit ScrollHandler(std::shared_ptr<Modifiers> mods_) : mods(std::move(mods_)) {
		proximity = xstate->in_proximity && prefs.proximity;
	}
	void raw_motion(CursorPosition e, bool abs_x, bool abs_y) override {
		if (proximity && !xstate->in_proximity) {
			parent->replace_child(nullptr);
			move_back();
		}
		if (!xstate->xinput_pressed.empty())
			AbstractScrollHandler::raw_motion(e, abs_x, abs_y);
	}
	void press_master(guint b, Time t) override {
		xstate->fake_core_button(b, false);
	}
	void release(guint b, CursorPosition e) override {
		if ((proximity && xstate->in_proximity) || !xstate->xinput_pressed.empty())
			return;
		parent->replace_child(0);
		move_back();
	}
	virtual std::string name() { return "Scroll"; }
	virtual Grabber::State grab_mode() { return Grabber::RAW; }
};

class ScrollAdvancedHandler : public AbstractScrollHandler {
    std::shared_ptr<Modifiers> m;
	guint &rb;
public:
	ScrollAdvancedHandler(std::shared_ptr<Modifiers> m_, guint &rb_) : m(std::move(m_)), rb(rb_) {}
	void fake_wheel(int b1, int n1, int b2, int n2) override {
		AbstractScrollHandler::fake_wheel(b1, n1, b2, n2);
		rb = 0;
	}
	void release(guint b, CursorPosition e) override {
		Handler *p = parent;
		p->replace_child(nullptr);
		p->release(b, e);
		move_back();
	}
	void press(guint b, CursorPosition e) override {
		Handler *p = parent;
		p->replace_child(nullptr);
		p->press(b, e);
		move_back();
	}
	std::string name() override { return "ScrollAdvanced"; }
	Grabber::State grab_mode() override { return Grabber::RAW; }
};

class AdvancedStrokeActionHandler : public Handler {
    std::shared_ptr<Gesture> s;
public:
	AdvancedStrokeActionHandler(std::shared_ptr<Gesture> s_, CursorPosition e) : s(std::move(s_)) {}
	void press(guint b, CursorPosition e) override {
		if (stroke_action) {
			s->button = b;
			(*stroke_action)(s);
		}
	}
	void release(guint b, CursorPosition e) override {
		if (stroke_action)
			(*stroke_action)(s);
		if (xstate->xinput_pressed.empty())
			parent->replace_child(nullptr);
	}
	std::string name() override { return "InstantStrokeAction"; }
	Grabber::State grab_mode() override { return Grabber::NONE; }
};

class AdvancedHandler : public Handler {
    CursorPosition e;
	guint remap_from, remap_to;
	Time click_time;
	guint replay_button;
    CursorPosition replay_orig;
	std::map<guint, std::shared_ptr<Actions::Action>> as;
	std::map<guint, double> rs;
	std::map<guint, std::shared_ptr<Modifiers>> mods;
    std::shared_ptr<Modifiers> sticky_mods;
	guint button1, button2;
    std::shared_ptr<std::vector<CursorPosition>> replay;

	void show_ranking(guint b, CursorPosition e) {
		if (!rs.count(b))
			return;
		rs.erase(b);
	}

	AdvancedHandler(std::shared_ptr<Gesture> s, CursorPosition e_, guint b1, guint b2, std::shared_ptr<std::vector<CursorPosition>> replay_) :
		e(e_), remap_from(0), remap_to(0), click_time(0), replay_button(0),
		button1(b1), button2(b2), replay(std::move(replay_)) {
        if (s) {
            actions.handle_advanced(*s, as, rs, b1, b2, grabber->current_class->get());
        }
    }

public:
	static Handler *create(std::shared_ptr<Gesture> s, CursorPosition e, guint b1, guint b2, std::shared_ptr<std::vector<CursorPosition>> replay) {
		if (stroke_action && s)
			return new AdvancedStrokeActionHandler(s, e);
		else
			return new AdvancedHandler(s, e, b1, b2, replay);

	}
	void init() override {
		if (replay && !replay->empty()) {
			bool replay_first = !as.count(button2);
			auto i = replay->begin();
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
	void press(guint b, CursorPosition e) override {
        if (xstate->current_dev->master)
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), e.x, e.y, 0);
        click_time = 0;
        if (remap_to) {
            xstate->fake_core_button(remap_to, false);
        }
        remap_from = 0;
        remap_to = 0;
        replay_button = 0;
        guint bb = (b == button1) ? button2 : b;
        show_ranking(bb, e);
        if (!as.count(bb)) {
            sticky_mods.reset();
            if (xstate->current_dev->master)
                context->xServer->fakeButtonEvent(b, true, CurrentTime);
            return;
        }
        auto act = as[bb];
        if (std::dynamic_pointer_cast<Actions::Scroll>(act)) {
            click_time = e.t;
            replay_button = b;
            replay_orig = e;
            auto m = act->prepare();
            sticky_mods.reset();
            return replace_child(new ScrollAdvancedHandler(m, replay_button));
        }
        if (std::dynamic_pointer_cast<Actions::Ignore>(act)) {
			click_time = e.t;
			replay_button = b;
			replay_orig = e;
		}
        if (auto b2 = Actions::Button::get_button(act)) {
			// This is kind of a hack:  Store modifiers in
			// sticky_mods, so that they are automatically released
			// on the next press
			sticky_mods = act->prepare();
			remap_from = b;
			remap_to = b2;
			xstate->fake_core_button(b2, true);
			return;
		}
		mods[b] = act->prepare();
		if (std::dynamic_pointer_cast<Actions::SendKey>(act)) {
			if (mods_equal(sticky_mods, mods[b]))
				mods[b] = sticky_mods;
			else
				sticky_mods = mods[b];
		} else {
            sticky_mods.reset();
        }

		act->run();
	}
	virtual void motion(CursorPosition e) {
        if (replay_button && hypot(replay_orig.x - e.x, replay_orig.y - e.y) > 16)
            replay_button = 0;
        if (xstate->current_dev->master)
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), e.x, e.y, 0);
    }
	virtual void release(guint b, CursorPosition e) {
        if (xstate->current_dev->master)
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), e.x, e.y, 0);
        if (remap_to) {
            xstate->fake_core_button(remap_to, false);
        }
        guint bb = (b == button1) ? button2 : b;
        if (!as.count(bb)) {
            sticky_mods.reset();
            if (xstate->current_dev->master)
                context->xServer->fakeButtonEvent(b, false, CurrentTime);
        }
        if (xstate->xinput_pressed.size() == 0) {
            if (e.t < click_time + 250 && b == replay_button) {
                sticky_mods.reset();
                mods.clear();
                xstate->fake_click(b);
            }
            return parent->replace_child(nullptr);
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
	guint trigger;
	std::shared_ptr<std::vector<CursorPosition>> cur;
	bool is_gesture;
	bool drawing;
	CursorPosition last, orig;
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
	typedef std::shared_ptr<Connection> RConnection;
	sigc::connection init_connection;
	std::vector<RConnection> connections;

	std::shared_ptr<Gesture> finish(guint b) {
        trace->end();
        context->xServer->flush();
        auto c = cur;
        if (!is_gesture || grabber->is_instant(button))
            c->clear();
        return std::make_shared<Gesture>(*c, trigger, b, xstate->modifiers, false);
    }

	bool timeout() {
        g_debug("Aborting stroke...");
        trace->end();
        auto c = cur;
        if (!is_gesture)
            c->clear();
        std::shared_ptr<Gesture> s;
        if (prefs.timeout_gestures || grabber->is_click_hold(button))
            s = std::make_shared<Gesture>(*c, trigger, 0, xstate->modifiers, true);
        parent->replace_child(AdvancedHandler::create(s, last, button, 0, cur));
        context->xServer->flush();
        return false;
    }

	void do_instant() {
		std::vector<CursorPosition> ps;
		auto s = std::make_shared<Gesture>(ps, trigger, button, xstate->modifiers, false);
		parent->replace_child(AdvancedHandler::create(s, orig, button, button, cur));
	}

	bool expired(RConnection c, double dist) {
		c->dist -= dist;
		return c->dist < 0;
	}
protected:
	void abort_stroke() {
		parent->replace_child(AdvancedHandler::create(std::shared_ptr<Gesture>(), last, button, 0, cur));
	}
	virtual void motion(CursorPosition e) {
		cur->push_back(e);
		float dist = hypot(e.x-orig.x, e.y-orig.y);
		if (!is_gesture && dist > 16) {
			if (use_timeout && !final_timeout)
				return abort_stroke();
			init_connection.disconnect();
			is_gesture = true;
		}
		if (!drawing && dist > 4 && (!use_timeout || final_timeout)) {
			drawing = true;
			bool first = true;
			for (auto i : *cur) {
				auto p = Trace::Point(i.x, i.y);
				if (first) {
					trace->start(p);
					first = false;
				} else {
					trace->draw(p);
				}
			}
		} else if (drawing) {
			trace->draw(Trace::Point(e.x, e.y));
		}
		if (use_timeout && is_gesture) {
			connections.erase(remove_if(connections.begin(), connections.end(),
						sigc::bind(sigc::mem_fun(*this, &StrokeHandler::expired),
							hypot(e.x - last.x, e.y - last.y))), connections.end());
			connections.push_back(RConnection(new Connection(this, radius, final_timeout)));
		}
		last = e;
	}

	void press(guint b, CursorPosition e) override {
		auto s = finish(b);
		parent->replace_child(AdvancedHandler::create(s, e, button, b, cur));
	}

	void release(guint b, CursorPosition e) override {
        auto s = finish(0);

        if (prefs.move_back && !xstate->current_dev->absolute)
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), orig.x, orig.y, 0);
        else
            context->xServer->fakeMotionEvent(context->xServer->getDefaultScreen(), e.x, e.y, 0);

        if (stroke_action) {
            (*stroke_action)(s);
            return parent->replace_child(nullptr);
        }
        auto act = actions.handle(*s, grabber->current_class->get());
        if (!act) {
            context->xServer->ringBell(None, 0, None);
            return parent->replace_child(nullptr);
        }
        auto mods = act->prepare();
        if (std::dynamic_pointer_cast<Actions::Click>(act)) {
            act = std::make_shared<Actions::Button>((Gdk::ModifierType) 0, b);
        } else if (auto b = Actions::Button::get_button(act)) {
            return parent->replace_child(new ButtonHandler(mods, b));
        }
        if (std::dynamic_pointer_cast<Actions::Ignore>(act)) {
            return parent->replace_child(new IgnoreHandler(mods));
        }
        if (std::dynamic_pointer_cast<Actions::Scroll>(act)) {
            return parent->replace_child(new ScrollHandler(mods));
        }
        act->run();
        parent->replace_child(nullptr);
    }
public:
	StrokeHandler(guint b, CursorPosition e) :
		button(b),
		trigger(grabber->get_default_button() == (int)b ? 0 : b),
		is_gesture(false),
		drawing(false),
		last(e),
		orig(e),
		init_timeout(prefs.init_timeout),
		final_timeout(prefs.final_timeout),
		radius(16)
	{
		auto dt = prefs.device_timeout;
		auto j = dt->find(xstate->current_dev->name);
		if (j != dt->end())
			get_timeouts(j->second, &init_timeout, &final_timeout);
		else
			get_timeouts(prefs.timeout_profile, &init_timeout, &final_timeout);
		use_timeout = init_timeout;
	}
	void init() override {
		if (grabber->is_instant(button))
			return do_instant();
		if (grabber->is_click_hold(button)) {
			use_timeout = true;
			init_timeout = 500;
			final_timeout = 0;
		}
		cur = std::make_shared<std::vector<CursorPosition>>();
		cur->push_back(orig);
		if (!use_timeout)
			return;
		if (final_timeout && final_timeout < 32 && radius < 16*32/final_timeout) {
			radius = 16*32/final_timeout;
			final_timeout = final_timeout*radius/16;
		}
		init_connection = Glib::signal_timeout().connect(
				sigc::mem_fun(*this, &StrokeHandler::timeout), init_timeout);
	}
	~StrokeHandler() override { trace->end(); }
	std::string name() override { return "Stroke"; }
	Grabber::State grab_mode() override { return Grabber::NONE; }
};

class IdleHandler : public Handler {
protected:
	virtual void init() {
		xstate->update_core_mapping();
	}
	virtual void press(guint b, CursorPosition e) {
		if (current_app_window.get())
			XState::activate_window(current_app_window.get(), e.t);
		replace_child(new StrokeHandler(b, e));
	}
public:
	explicit IdleHandler(XState *xstate_) {
		xstate = xstate_;
	}
	~IdleHandler() override {
        context->xServer->ungrabKey(context->xServer->keysymToKeycode(XK_Escape), AnyModifier, context->xServer->ROOT);
	}
	std::string name() override { return "Idle"; }
	Grabber::State grab_mode() override { return Grabber::BUTTON; }
};

XState::XState() : current_dev(nullptr), in_proximity(false), accepted(true), modifiers(0) {
    int n, opcode, event, error;
    char **ext = context->xServer->listExtensions(&n);
    for (int i = 0; i < n; i++)
        if (context->xServer->queryExtension(ext[i], &opcode, &event, &error))
            opcodes[opcode] = ext[i];
    XServerProxy::freeExtensionList(ext);
    oldHandler = XSetErrorHandler(xErrorHandler);
    oldIOHandler = XSetIOErrorHandler(xIOErrorHandler);
    ping_window = context->xServer->createSimpleWindow(context->xServer->ROOT, 0, 0, 1, 1, 0, 0, 0);
    handler = new IdleHandler(this);
    handler->init();
}

void XState::run_action(const std::shared_ptr<Actions::Action>& act) {
    auto mods = act->prepare();
    if (auto b = Actions::Button::get_button(act)) {
        return handler->replace_child(new ButtonHandler(mods, b));
    }

    if (std::dynamic_pointer_cast<Actions::Ignore>(act)) {
        return handler->replace_child(new IgnoreHandler(mods));
    }

    if (std::dynamic_pointer_cast<Actions::Scroll>(act)) {
        return handler->replace_child(new ScrollHandler(mods));
    }
    act->run();
}
