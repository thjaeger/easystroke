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

class Grabber {
public:
	Children children;
	enum State { NONE, BUTTON, ALL_SYNC, ALL_ASYNC, POINTER };
	enum EventType { DOWN = 0, UP = 1, MOTION = 2, BUTTON_MOTION = 3, PROX_IN = 4, PROX_OUT = 5 };
	static const char *state_name[5];
	bool xinput;
	bool proximity_selected;
	bool is_event(int, EventType);
	unsigned int get_device_button_state();
	void select_proximity();

	struct XiDevice {
		XDevice *dev;
		XEventClass events[6];
		int event_type[6];
		int all_events_n;
		bool supports_proximity, supports_pressure;
		int pressure_min, pressure_max;
		int min_x, max_x, min_y, max_y;
		int normalize_pressure(int pressure) {
			return 255 * (pressure - pressure_min) / (pressure_max - pressure_min);
		}
	};
	XiDevice *get_xi_dev(XID id);
	int event_presence;
	XEventClass presence_class;
private:
	int button_events_n;
	int all_events_n; // TODO: Rename
	XiDevice **xi_devs;
	int xi_devs_n;
	bool init_xi();

	State current, grabbed;
	bool xi_grabbed;
	bool suspended;
	bool active;
	Cursor cursor;
	std::map<guint, guint> buttons;
	bool timing_workaround;

	Atom WM_STATE;

	void set();
	std::string get_wm_state(Window w);
	void grab_xi(bool);
public:
	Grabber();
	~Grabber();
	bool handle(XEvent &ev) { return children.handle(ev); }
	void update(Window w) { { Atomic a; active = !prefs.exceptions.ref(a).count(get_wm_state(w)); } set(); }

	void get_button();
	void fake_button(int b);
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
