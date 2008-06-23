#ifndef __GRABBER_H__
#define __GRABBER_H__
#include "prefdb.h"
#include <string>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>

class Grabber {
	enum Goal { NONE, BUTTON, ALL, XI };
	struct State {
		bool grab;
		bool suspend;
		bool xi;
		bool all; // Takes precedence over suspend
	};

public:
	bool xinput;
	bool is_button_up(int);
private:
	struct XiDevice {
		int button_down;
		int button_up;
		XDevice *dev;
		XEventClass button_events[2];
		int button_events_n;
	};
	XiDevice **xi_devs;
	int xi_devs_n;
	bool init_xi();

	State current;

	sigc::slot<void> before;
	sigc::slot<void> after;

public:
	unsigned int button;
private:
	unsigned int state;

	Atom wm_state;

	void set(State);
	static Goal goal(State s) {
		if (s.all)
			return ALL;
		if (s.xi)
			return XI;
		if (s.suspend)
			return NONE;
		return s.grab ? BUTTON : NONE;
	}
	std::string get_wm_state(Window w);
public:
	Grabber();
	void init(Window w, int depth);
	bool has_wm_state(Window w);
	void update(Window w) { grab(!RPrefEx(prefs().exceptions)->count(get_wm_state(w))); }
	void create(Window w);
	void get_button();
	void fake_button();
	void ignore(int b);
	void grab(bool grab = true) { State s = current; s.grab = grab; set(s); }
	void grab_all(sigc::slot<void> before_, sigc::slot<void> after_) {
		State s = current; s.all = true; set(s);
		before = before_;
		after = after_;
	}
	void grab_xi(bool grab = true) { State s = current; s.xi = grab; set(s); }
	void suspend() { State s = current; s.suspend = true; set(s); }
	void restore() { State s = current; s.suspend = false; set(s); }
	void regrab() { suspend(); get_button(); restore(); }
};

#endif
