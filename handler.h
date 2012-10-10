/*
 * Copyright (c) 2012, Thomas Jaeger <ThJaeger@gmail.com>
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
#ifndef __HANDLER_H__
#define __HANDLER_H__
#include "gesture.h"
#include "grabber.h"
#include "actiondb.h"

class Handler;

class XState {
	friend class Handler;
public:
	XState();

	bool handle(Glib::IOCondition);
	void handle_enter_leave(XEvent &ev);
	void handle_event(XEvent &ev);
	void handle_xi2_event(XIDeviceEvent *event);
	void handle_raw_motion(XIRawEvent *event);
	void report_xi2_event(XIDeviceEvent *event, const char *type);

	void fake_core_button(guint b, bool press);
	void fake_click(guint b);
	void update_core_mapping();

	void remove_device(int deviceid);
	void ungrab(int deviceid);

	bool idle();
	void ping();
	void bail_out();
	void select();
	void run_action(RAction act);
	void queue(sigc::slot<void> f);
	std::string select_window();

	static void activate_window(Window w, Time t);
	static Window get_window(Window w, Atom prop);
	static Atom get_atom(Window w, Atom prop);
	static bool has_atom(Window w, Atom prop, Atom value);
	static void icccm_client_message(Window w, Atom a, Time t);

	Grabber::XiDevice *current_dev;
	bool in_proximity;
	bool accepted;
	std::set<guint> xinput_pressed;
	guint modifiers;
        std::map<guint, guint> core_inv_map;
private:
	Window ping_window;
	Handler *handler;

	static int xErrorHandler(Display *dpy2, XErrorEvent *e);
	static int xIOErrorHandler(Display *dpy2);
	int (*oldHandler)(Display *, XErrorEvent *);
	int (*oldIOHandler)(Display *);
	std::list<sigc::slot<void> > queued;
	std::map<int, std::string> opcodes;
};

class Handler {
public:
	Handler *child;
	Handler *parent;
	Handler() : child(NULL), parent(NULL) {}
	Handler *top() {
		if (child)
			return child->top();
		else
			return this;
	}

	virtual void motion(RTriple e) {}
	virtual void raw_motion(RTriple e, bool, bool) {}
	virtual void press(guint b, RTriple e) {}
	virtual void release(guint b, RTriple e) {}
	virtual void press_master(guint b, Time t) {}
	virtual void pong() {}
	void replace_child(Handler *c);
	virtual void init() {}
	virtual ~Handler() {
		if (child)
			delete child;
	}
	virtual std::string name() = 0;
	virtual Grabber::State grab_mode() = 0;
};

extern XState *xstate;

#endif
