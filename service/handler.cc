#include "handler.h"
#include "log.h"
#include "xserverproxy.h"
#include "trace.h"
#include <X11/Xutil.h>
#include "actions/modAction.h"
#include <memory>
#include <utility>
#include "eventloop.h"

std::shared_ptr<sigc::slot<void, std::shared_ptr<Gesture>> > stroke_action;

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
	global_grabber->grab(new_handler->grab_mode());
	if (child)
		child->init();
	while (!global_eventLoop->queued.empty() && global_eventLoop->idle()) {
		(*global_eventLoop->queued.begin())();
		global_eventLoop->queued.pop_front();
	}
}

class IgnoreHandler : public Handler {
    std::shared_ptr<Modifiers> mods;
	bool proximity;
public:
	explicit IgnoreHandler(std::shared_ptr<Modifiers> mods_) : mods(std::move(mods_)), proximity(global_eventLoop->in_proximity && prefs.proximity) {}
	void press(guint b, CursorPosition e) override {
		if (global_eventLoop->current_dev->master) {
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), e.x, e.y, 0);
            global_xServer->fakeButtonEvent(b, true, CurrentTime);
        }
	}
	void motion(CursorPosition e) override {
        if (global_eventLoop->current_dev->master)
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), e.x, e.y, 0);
        if (proximity && !global_eventLoop->in_proximity)
            parent->replace_child(nullptr);
    }
	void release(guint b, CursorPosition e) override {
		if (global_eventLoop->current_dev->master) {
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), e.x, e.y, 0);
            global_xServer->fakeButtonEvent(b, false, CurrentTime);
        }
		if (proximity ? !global_eventLoop->in_proximity : global_eventLoop->xinput_pressed.empty())
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
		proximity(global_eventLoop->in_proximity && prefs.proximity)
	{}
	void press(guint b, CursorPosition e) override {
		if (global_eventLoop->current_dev->master) {
            if (!real_button)
                real_button = b;
            if (real_button == b)
                b = button;
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), e.x, e.y, 0);
            global_xServer->fakeButtonEvent(b, true, CurrentTime);
        }
	}
	void motion(CursorPosition e) override {
        if (global_eventLoop->current_dev->master)
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), e.x, e.y, 0);
        if (proximity && !global_eventLoop->in_proximity)
            parent->replace_child(nullptr);
    }
	void release(guint b, CursorPosition e) override {
		if (global_eventLoop->current_dev->master) {
            if (real_button == b)
                b = button;
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), e.x, e.y, 0);
            global_xServer->fakeButtonEvent(b, false, CurrentTime);
        }
		if (proximity ? !global_eventLoop->in_proximity : global_eventLoop->xinput_pressed.empty())
			parent->replace_child(nullptr);
	}
	std::string name() override { return "Button"; }
	Grabber::State grab_mode() override { return Grabber::NONE; }
};

class AbstractScrollHandler : public Handler {
	bool have_x, have_y;
    double last_x, last_y;
	Time last_t;
    double offset_x, offset_y;
	Glib::ustring str;
	int orig_x, orig_y;

protected:
	AbstractScrollHandler() : have_x(false), have_y(false), last_x(0.0), last_y(0.0), last_t(0), offset_x(0.0), offset_y(0.0) {
        if (!prefs.move_back || (global_eventLoop->current_dev && global_eventLoop->current_dev->absolute))
            return;
        Window dummy1, dummy2;
        int dummy3, dummy4;
        unsigned int dummy5;
        global_xServer->queryPointer(global_xServer->ROOT, &dummy1, &dummy2, &orig_x, &orig_y, &dummy3, &dummy4, &dummy5);
    }
	virtual void fake_wheel(int b1, int n1, int b2, int n2) {
		for (int i = 0; i<n1; i++)
			global_eventLoop->fake_click(b1);
		for (int i = 0; i<n2; i++)
			global_eventLoop->fake_click(b2);
	}
	static float curve(float v) {
		return v * exp(log(abs(v))/3);
	}
protected:
	void move_back() const {
        if (!prefs.move_back || (global_eventLoop->current_dev && global_eventLoop->current_dev->absolute))
            return;
        global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), orig_x, orig_y, 0);
    }
public:
	void raw_motion(CursorPosition e, bool abs_x, bool abs_y) override {
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
		proximity = global_eventLoop->in_proximity && prefs.proximity;
	}
	void raw_motion(CursorPosition e, bool abs_x, bool abs_y) override {
		if (proximity && !global_eventLoop->in_proximity) {
			parent->replace_child(nullptr);
			move_back();
		}
		if (!global_eventLoop->xinput_pressed.empty())
			AbstractScrollHandler::raw_motion(e, abs_x, abs_y);
	}
	void press_master(guint b, Time t) override {
		global_eventLoop->fake_core_button(b, false);
	}
	void release(guint b, CursorPosition e) override {
		if ((proximity && global_eventLoop->in_proximity) || !global_eventLoop->xinput_pressed.empty())
			return;
		parent->replace_child(0);
		move_back();
	}
	std::string name() override { return "Scroll"; }
	Grabber::State grab_mode() override { return Grabber::RAW; }
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
		if (global_eventLoop->xinput_pressed.empty())
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
            actions.handle_advanced(*s, as, rs, b1, b2, Events::WindowObserver::getCurrentWindowClass());
        }
    }

public:
	static Handler *create(std::shared_ptr<Gesture> s, CursorPosition e, guint b1, guint b2, std::shared_ptr<std::vector<CursorPosition>> replay) {
		if (stroke_action && s)
			return new AdvancedStrokeActionHandler(s, e);
		else
			return new AdvancedHandler(s, e, b1, b2, std::move(replay));

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
        if (global_eventLoop->current_dev->master)
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), e.x, e.y, 0);
        click_time = 0;
        if (remap_to) {
            global_eventLoop->fake_core_button(remap_to, false);
        }
        remap_from = 0;
        remap_to = 0;
        replay_button = 0;
        guint bb = (b == button1) ? button2 : b;
        show_ranking(bb, e);
        if (!as.count(bb)) {
            sticky_mods.reset();
            if (global_eventLoop->current_dev->master)
                global_xServer->fakeButtonEvent(b, true, CurrentTime);
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
			global_eventLoop->fake_core_button(b2, true);
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
	void motion(CursorPosition e) override {
        if (replay_button && hypot(replay_orig.x - e.x, replay_orig.y - e.y) > 16)
            replay_button = 0;
        if (global_eventLoop->current_dev->master)
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), e.x, e.y, 0);
    }
	void release(guint b, CursorPosition e) override {
        if (global_eventLoop->current_dev->master)
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), e.x, e.y, 0);
        if (remap_to) {
            global_eventLoop->fake_core_button(remap_to, false);
        }
        guint bb = (b == button1) ? button2 : b;
        if (!as.count(bb)) {
            sticky_mods.reset();
            if (global_eventLoop->current_dev->master)
                global_xServer->fakeButtonEvent(b, false, CurrentTime);
        }
        if (global_eventLoop->xinput_pressed.empty()) {
            if (e.t < click_time + 250 && b == replay_button) {
                sticky_mods.reset();
                mods.clear();
                global_eventLoop->fake_click(b);
            }
            return parent->replace_child(nullptr);
        }
        replay_button = 0;
        mods.erase((b == button1) ? button2 : b);
        if (remap_from)
            sticky_mods.reset();
        remap_from = 0;
    }
	std::string name() override { return "Advanced"; }
	Grabber::State grab_mode() override { return Grabber::NONE; }
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
        global_xServer->flush();
        auto c = cur;
        if (!is_gesture || global_grabber->is_instant(button))
            c->clear();
        return std::make_shared<Gesture>(*c, trigger, b, global_eventLoop->modifiers, false);
    }

	bool timeout() {
        g_debug("Aborting stroke...");
        trace->end();
        auto c = cur;
        if (!is_gesture)
            c->clear();
        std::shared_ptr<Gesture> s;
        if (prefs.timeout_gestures || global_grabber->is_click_hold(button))
            s = std::make_shared<Gesture>(*c, trigger, 0, global_eventLoop->modifiers, true);
        parent->replace_child(AdvancedHandler::create(s, last, button, 0, cur));
        global_xServer->flush();
        return false;
    }

    void do_instant() {
        std::vector<CursorPosition> ps;
        auto s = std::make_shared<Gesture>(ps, trigger, button, global_eventLoop->modifiers, false);
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

    void motion(CursorPosition e) override {
        cur->push_back(e);
        float dist = hypot(e.x - orig.x, e.y - orig.y);
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
			connections.erase(
			        remove_if(connections.begin(), connections.end(), sigc::bind(sigc::mem_fun(*this, &StrokeHandler::expired), hypot(e.x - last.x, e.y - last.y))),
			        connections.end());
			connections.push_back(std::make_shared<Connection>(this, radius, final_timeout));
		}
		last = e;
	}

	void press(guint b, CursorPosition e) override {
		auto s = finish(b);
		parent->replace_child(AdvancedHandler::create(s, e, button, b, cur));
	}

	void release(guint b, CursorPosition e) override {
        auto s = finish(0);

        if (prefs.move_back && !global_eventLoop->current_dev->absolute)
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), orig.x, orig.y, 0);
        else
            global_xServer->fakeMotionEvent(global_xServer->getDefaultScreen(), e.x, e.y, 0);

        if (stroke_action) {
            (*stroke_action)(s);
            return parent->replace_child(nullptr);
        }
        auto act = actions.handle(*s, Events::WindowObserver::getCurrentWindowClass());
        if (!act) {
            global_xServer->ringBell(None, 0, None);
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
		trigger(global_grabber->get_default_button() == (int)b ? 0 : b),
		is_gesture(false),
		drawing(false),
		last(e),
		orig(e),
		init_timeout(prefs.init_timeout),
		final_timeout(prefs.final_timeout),
		radius(16)
	{
		auto dt = prefs.device_timeout;
		auto j = dt->find(global_eventLoop->current_dev->name);
		if (j != dt->end())
			get_timeouts(j->second, &init_timeout, &final_timeout);
		else
			get_timeouts(prefs.timeout_profile, &init_timeout, &final_timeout);
		use_timeout = init_timeout;
	}
	void init() override {
		if (global_grabber->is_instant(button))
			return do_instant();
		if (global_grabber->is_click_hold(button)) {
            use_timeout = true;
            init_timeout = 500;
            final_timeout = 0;
        }
        cur = std::make_shared<std::vector<CursorPosition>>();
        cur->push_back(orig);
        if (!use_timeout)
            return;
        if (final_timeout && final_timeout < 32 && radius < 16 * 32 / final_timeout) {
            radius = 16 * 32 / final_timeout;
            final_timeout = final_timeout * radius / 16;
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
	void init() override {
		global_eventLoop->update_core_mapping();
	}
	void press(guint b, CursorPosition e) override {
	    Events::WindowObserver::tryActivateCurrentWindow(e.t);
		replace_child(new StrokeHandler(b, e));
	}
public:
	explicit IdleHandler(EventLoop *xstate_) {
        global_eventLoop = xstate_;
	}
	~IdleHandler() override {
        global_xServer->ungrabKey(global_xServer->keysymToKeycode(XK_Escape), AnyModifier, global_xServer->ROOT);
	}
	std::string name() override { return "Idle"; }
	Grabber::State grab_mode() override { return Grabber::BUTTON; }
};

std::unique_ptr<Handler> HandlerFactory::makeIdleHandler(EventLoop *xstate_) {
    return std::make_unique<IdleHandler>(xstate_);
}

void Handler::runAction(const std::shared_ptr<Actions::Action>& act) {
    auto mods = act->prepare();
    if (auto b = Actions::Button::get_button(act)) {
        return this->replace_child(new ButtonHandler(mods, b));
    }

    if (std::dynamic_pointer_cast<Actions::Ignore>(act)) {
        return this->replace_child(new IgnoreHandler(mods));
    }

    if (std::dynamic_pointer_cast<Actions::Scroll>(act)) {
        return this->replace_child(new ScrollHandler(mods));
    }
    act->run();
}
