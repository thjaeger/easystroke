#ifndef __GRABBER_H__
#define __GRABBER_H__
#include "prefdb.h"
#include <string>
#include <map>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/cursorfont.h>


class Grabber {
public:
	enum State { NONE, BUTTON, ALL_SYNC, ALL_ASYNC, POINTER };
	static const char *state_name[5];
	bool xinput;
	bool is_button_up(int);
	bool is_button_down(int);
	bool is_motion(int);
	unsigned int get_device_button_state();
private:
	struct XiDevice {
		int button_down;
		int button_up;
		int button_motion;
		XDevice *dev;
		XEventClass events[4];
	};
	int button_events_n;
	int all_events_n;
	XiDevice **xi_devs;
	int xi_devs_n;
	bool init_xi();

	State current, grabbed;
	bool suspended;
	bool active;
	Cursor cursor;
	std::map<guint, guint> buttons;
	bool timing_workaround;

	Atom wm_state;

	void set();
	std::string get_wm_state(Window w);
	void grab_xi(bool);
public:
	Grabber();
	~Grabber();
	void init(Window w, int depth);
	bool has_wm_state(Window w);
	void update(Window w) { active = !RPrefEx(prefs().exceptions)->count(get_wm_state(w)); set(); }
	void create(Window w);
	void get_button();
	void fake_button(int b);
	void ignore(int b);
	void grab(State s) { current = s; set(); }
	void suspend() { suspended = true; set(); }
	void resume() { suspended = false; set(); }
	void regrab() { suspend(); grab_xi(false); get_button(); resume(); grab_xi(true); }
	void grab_xi_devs(bool);
	bool is_grabbed(guint b) { return buttons.find(b) != buttons.end(); }
};

class GrabFailedException : public std::exception {
	virtual const char* what() const throw() { return "Grab Failed"; }
};

#endif
