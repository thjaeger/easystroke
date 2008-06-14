#ifndef __GRABBER_H__
#define __GRABBER_H__
#include "prefdb.h"
#include <map>
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


	bool init_xi();
	bool xinput;
	XDevice *xi_dev;
	XEventClass button_events[4];
	int button_events_n;
//	int motion_notify;
	int button_down;
	int button_up;

	std::map<Window, std::string> wins;
	State current;

	sigc::slot<void> before;
	sigc::slot<void> after;

	unsigned int button;
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
public:
	Grabber();
	void init(Window w, int depth);
	bool has_wm_state(Window w);
	void update(Window w) { grab(!RPrefEx(prefs().exceptions)->count(wins[w])); }
	void create(Window w);
	void destroy(Window w) { wins.erase(w); }
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
