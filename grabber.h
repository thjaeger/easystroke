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

void queue(sigc::slot<void> f);
std::string select_window();

class Grabber {
	friend class Handler;
	friend class StrokeHandler;
public:
	Children children;
	enum State { NONE, BUTTON, ALL_SYNC, SELECT };
	static const char *state_name[6];
	enum EventType { DOWN = 0, UP = 1, MOTION = 2, BUTTON_MOTION = 3, PROX_IN = 4, PROX_OUT = 5 };
	bool xinput;
	bool proximity_selected;
	int event_type[6];
	bool is_event(int type, EventType et) { return xinput && type == event_type[et]; }
	void select_proximity();

	struct XiDevice {
		std::string name;
		std::map<guint, guint> inv_map;
		XDevice *dev;
		XEventClass events[6];
		int all_events_n;
		bool supports_proximity, supports_pressure;
		bool active;
		int pressure_min, pressure_max;
		int min_x, max_x, min_y, max_y;
		bool absolute;
		int valuators[2];
		int num_buttons;
		int normalize_pressure(int pressure) {
			return 255 * (pressure - pressure_min) / (pressure_max - pressure_min);
		}
		void fake_button(int b, bool press);
		void release_all();
		void grab_device(bool);
		void update_pointer_mapping();
		void translate_coords(int *axis_data, float &x, float &y);
		bool translate_known_coords(int sx, int sy, int *axis_data, float &x, float &y);
	};

	unsigned int get_device_button_state(XiDevice *&dev);
	XiDevice *get_xi_dev(XID id);
	int mapping_notify;
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
	bool xi_devs_grabbed;
	int suspended;
	bool active;
	Cursor cursor_select;
	ButtonInfo grabbed_button;
	std::vector<ButtonInfo> buttons;
	bool timing_workaround;

	void set();
	void grab_xi(bool);
	void grab_xi_devs(bool);
	std::string get_wm_class(Window w);
	std::string wm_class;

	void update_excluded();

	void grab(State s) { current = s; set(); }
	void suspend() { suspended++; set(); }
	void resume() { if (suspended) suspended--; set(); }
	void update();
public:
	Grabber();
	~Grabber();
	bool handle(XEvent &ev) { return children.handle(ev); }
	std::string get_wm_class() { return wm_class; }

	void queue_suspend() { queue(sigc::mem_fun(*this, &Grabber::suspend)); }
	void queue_resume() { queue(sigc::mem_fun(*this, &Grabber::resume)); }

	bool update_device_list();

	bool is_grabbed(guint b);
	bool is_instant(guint b);
	void release_all(int n = 0);

	int get_default_button() { return grabbed_button.button; }
	bool get_timing_workaround() { return timing_workaround; }

	void unminimize();
};

class GrabFailedException : public std::exception {
	virtual const char* what() const throw();
};

#endif
