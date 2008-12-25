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
#ifndef __GRABBER_H__
#define __GRABBER_H__
#include "prefdb.h"
#include <string>
#include <map>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/cursorfont.h>

class XAtom {
	const char *name;
	Atom atom;

public:
	XAtom(const char *name_) : name(name_), atom(0) {}
	Atom operator*();
	Atom operator->() { return operator*(); }
};

class Children {
	Window parent;
public:
	Children(Window);
	bool handle(XEvent &ev);
	void add(Window);
	void remove(Window);
	void destroy(Window);
};

class Grabber;
extern Grabber *grabber;

float rescaleValuatorAxis(int coord, int fmin, int fmax, int tmax);
bool has_wm_state(Window w);
bool has_atom(Window w, Atom prop, Atom value);

class Grabber {
public:
	Children children;
	enum State { NONE, BUTTON, ALL_SYNC, SELECT };
	static const char *state_name[6];
	enum EventType { DOWN = 0, UP = 1, MOTION = 2, BUTTON_MOTION = 3, PROX_IN = 4, PROX_OUT = 5 };
	bool xinput;
	bool proximity_selected;
	bool is_event(int, EventType);
	void select_proximity();

	struct XiDevice {
		std::string name;
		XDevice *dev;
		XEventClass events[6];
		int event_type[6];
		int all_events_n;
		bool supports_proximity, supports_pressure;
		int pressure_min, pressure_max;
		int min_x, max_x, min_y, max_y;
		bool absolute;
		int normalize_pressure(int pressure) {
			return 255 * (pressure - pressure_min) / (pressure_max - pressure_min);
		}
		unsigned int get_button_state();
		void fake_press(int b, int core);
		void fake_release(int b, int core);
	};
	XiDevice *get_xi_dev(XID id);
	int event_presence;
	XEventClass presence_class;

	XiDevice **xi_devs;
	int xi_devs_n;
	int nMajor;
private:
	int button_events_n;
	bool init_xi();

	State current, grabbed;
	bool xi_grabbed;
	bool suspended, xi_suspended;
	bool disabled;
	bool active;
	Cursor cursor_select;
	ButtonInfo grabbed_button;
	std::map<guint, guint> buttons;
	bool timing_workaround;

	void set();
	void grab_xi(bool);
	std::string get_wm_class(Window w);
	std::string wm_class;
public:
	Grabber();
	~Grabber();
	bool handle(XEvent &ev) { return children.handle(ev); }
	void update(Window w);
	std::string get_wm_class() { return wm_class; }

	void fake_button(int b);
	void grab(State s) { current = s; set(); }
	void suspend() { suspended = true; set(); }
	void resume() { suspended = false; set(); }
	void xi_suspend() { xi_suspended = true; set(); }
	void xi_resume() { xi_suspended = false; set(); }
	void update_button(ButtonInfo bi);
	void grab_xi_devs(bool);
	bool is_grabbed(guint b) { return buttons.find(b) != buttons.end(); }
	void toggle_disabled() { disabled = !disabled; set(); }
	bool update_device_list();

	void unminimize();
};

class GrabFailedException : public std::exception {
	virtual const char* what() const throw() { return "Grab Failed"; }
};

#endif
