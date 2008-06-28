#ifndef __GRABBER_H__
#define __GRABBER_H__
#include "prefdb.h"
#include <string>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/cursorfont.h>

class Grabber {
	enum Goal { NONE, BUTTON, ALL, XI, POINTER };
	struct State {
		bool grab;
		bool suspend;
		bool xi;
		bool pointer;
		bool all; // Takes precedence over suspend
	};

public:
	bool xinput;
	bool is_button_up(int);
	bool is_button_down(int);
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
	Cursor cursor;

public:
	unsigned int button;
private:
	unsigned int state;

	Atom wm_state;

	void set(State);
	static Goal goal(State s) {
		if (s.all)
			return ALL;
		if (s.suspend)
			return NONE;
		if (s.pointer)
			return POINTER;
		if (s.xi)
			return XI;
		return s.grab ? BUTTON : NONE;
	}
	std::string get_wm_state(Window w);
public:
	Grabber();
	~Grabber();
	void init(Window w, int depth);
	bool has_wm_state(Window w);
	void update(Window w) { grab(!RPrefEx(prefs().exceptions)->count(get_wm_state(w))); }
	void create(Window w);
	void get_button();
	void fake_button(int b);
	void ignore(int b);
	void grab(bool grab = true) { State s = current; s.grab = grab; set(s); }
	void grab_all() {
		State s = current; s.all = true; set(s);
	}
	void grab_xi(bool grab = true) { State s = current; s.xi = grab; set(s); }
	void grab_pointer(bool grab = true) { State s = current; s.pointer = grab; set(s); }
	void suspend() { State s = current; s.suspend = true; set(s); }
	void restore() { State s = current; s.suspend = false; set(s); }
	void regrab() { suspend(); get_button(); restore(); }
};

#endif
