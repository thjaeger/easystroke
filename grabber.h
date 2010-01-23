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
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>

#define MAX_BUTTONS 256

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

bool has_wm_state(Window w);
bool has_atom(Window w, Atom prop, Atom value);

void queue(sigc::slot<void> f);
std::string select_window();
void fake_core_button(guint b, bool press);

class Grabber {
	friend class Handler;
	friend class StrokeHandler;
	friend class Button;
	friend class Prefs;
public:
	Children children;
	enum State { NONE, BUTTON, SELECT };
	static const char *state_name[3];

	struct XiDevice {
		int dev;
		std::string name;
		bool supports_pressure;
		bool absolute;
		bool active;
		int pressure_min, pressure_max;
		int num_buttons;
		int master;
		XiDevice(Grabber *, XIDeviceInfo *);
		int normalize_pressure(int pressure) {
			return 255 * (pressure - pressure_min) / (pressure_max - pressure_min);
		}
		void grab_device(bool grab);
		void grab_button(ButtonInfo &bi, bool grab);
		void fake_motion(int x, int y);
	};

	typedef std::map<XID, boost::shared_ptr<XiDevice> > DeviceMap;
	int opcode, event, error;
	XiDevice *get_xi_dev(int id);
private:
	bool init_xi();

	DeviceMap xi_devs;
	State current, grabbed;
	bool xi_grabbed;
	bool xi_devs_grabbed;
	int suspended;
	bool active;
	Cursor cursor_select;
	ButtonInfo grabbed_button;
	std::vector<ButtonInfo> buttons;

	void set();
	void grab_xi(bool);
	void grab_xi_devs(bool);

	void update_excluded();

	void grab(State s) { current = s; set(); }
	void suspend() { suspended++; set(); }
	void resume() { if (suspended) suspended--; set(); }
	void update();
public:
	Grabber();
	~Grabber();
	bool handle(XEvent &ev) { return children.handle(ev); }
	Out<std::string> *current_class;

	void queue_suspend() { queue(sigc::mem_fun(*this, &Grabber::suspend)); }
	void queue_resume() { queue(sigc::mem_fun(*this, &Grabber::resume)); }

	void new_device(XIDeviceInfo *);

	bool is_grabbed(guint b);
	bool is_instant(guint b);
	bool is_click_hold(guint b);
	void hierarchy_changed(XIHierarchyEvent *);

	int get_default_button() { return grabbed_button.button; }

	void unminimize();
};

extern Grabber::XiDevice *current_dev;
extern std::set<guint> xinput_pressed;

class GrabFailedException : public std::exception {
	char *msg;
public:
	GrabFailedException(int code) { if (asprintf(&msg, "Grab Failed: %d", code) == -1) msg = NULL; }
	virtual const char* what() const throw() { return msg ? msg : "Grab Failed"; }
	~GrabFailedException() throw() { free(msg); }
};

#endif
