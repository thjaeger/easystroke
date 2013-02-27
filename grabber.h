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

class Grabber {
	friend class Handler;
	friend class StrokeHandler;
	friend class Button;
	friend class Prefs;
public:
	Children children;
	enum State { BUTTON, GRAB, SELECT, RAW };
	enum GrabState { GrabNo, GrabYes, GrabRaw };
	static const char *state_name[4];

	struct XiDevice {
		int dev;
		std::string name;
		bool absolute;
		bool active;
		int proximity_axis;
		bool touch;
		double scale_x, scale_y;
		int num_buttons;
		int master;
		XiDevice(Grabber *, XIDeviceInfo *);

		void update_grabs();
		void update_button(ButtonInfo &bi, bool grab);
		bool button_grabbed;
		GrabState device_grabbed;
	};

	typedef std::map<XID, boost::shared_ptr<XiDevice> > DeviceMap;
	int opcode, event, error;
	XiDevice *get_xi_dev(int id);
private:
	bool init_xi();

	DeviceMap xi_devs;
	State current;
	bool select;
	bool grab_button, grab_touch;
	GrabState grab_device;
	bool touch_grabbed;
	int suspended;
	bool active;
	Cursor cursor_select;
	ButtonInfo grabbed_button;
	std::vector<ButtonInfo> buttons;

	void update_grabs();
	void set();

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

	void queue_suspend();
	void queue_resume();
	std::string select_window();

	void new_device(XIDeviceInfo *);

	bool has_touch_grab() { return grab_touch && grab_device != GrabYes; }
	bool is_active() { return active; }
	bool is_instant(guint b);
	bool is_click_hold(guint b);
	bool hierarchy_changed(XIHierarchyEvent *);

	int get_default_button() { return grabbed_button.button; }
	guint get_default_mods(guint button);

	void unminimize();
};

class GrabFailedException : public std::exception {
	char *msg;
public:
	GrabFailedException(int code) { if (asprintf(&msg, "Grab Failed: %d", code) == -1) msg = NULL; }
	virtual const char* what() const throw() { return msg ? msg : "Grab Failed"; }
	~GrabFailedException() throw() { free(msg); }
};

#endif
