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
#include <gtkmm.h>
#include <glibmm/i18n.h>
#include <string>
#include <string.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XTest.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

extern Display *dpy;
#define ROOT (DefaultRootWindow(dpy))

int press, release;

class Grabber {
public:
	enum State { NONE, BUTTON };

	XDevice *dev;
	XEventClass events[4];
	int event_type[4];
	int all_events_n;
	int button_events_n;

	State current, grabbed;
	bool xi_devs_grabbed;

	void set();
	void grab_xi_devs(bool);
	Grabber();

	void grab(State s) { current = s; set(); }
};

Grabber *grabber = 0;

Grabber::Grabber() {
	current = BUTTON;
	grabbed = NONE;
	xi_devs_grabbed = false;
	int n;
	XDeviceInfo *devs = XListInputDevices(dpy, &n);
	if (!devs)
		exit(EXIT_FAILURE);

	for (int i = 0; i < n; i++) {
		XDeviceInfo *dev_info = devs + i;

		if (strcmp(dev_info->name, "stylus"))
			continue;

		dev = XOpenDevice(dpy, dev_info->id);
		break;
	}
	XFreeDeviceList(devs);
	if (!dev)
		exit(EXIT_FAILURE);
	int dummy;
	DeviceButtonPress(dev, press, events[0]);
	DeviceButtonRelease(dev, release, events[1]);
	DeviceButtonMotion(dev, dummy, events[2]);
	button_events_n = 3;
	DeviceMotionNotify(dev, dummy, events[3]);
	all_events_n = 4;
	XGrabDeviceButton(dpy, dev, 1, 0, NULL, ROOT, False, button_events_n, events, GrabModeAsync, GrabModeAsync);
	set();
}

void Grabber::grab_xi_devs(bool grab) {
	if (!xi_devs_grabbed == !grab)
		return;
	xi_devs_grabbed = grab;
	if (grab)
		XGrabDevice(dpy, dev, ROOT, False, all_events_n, events, GrabModeAsync, GrabModeAsync, CurrentTime);
	else
		XUngrabDevice(dpy, dev, CurrentTime);
}

void Grabber::set() {
	grab_xi_devs(current == NONE);
	State old = grabbed;
	grabbed = current;
	if (old == grabbed)
		return;
	if (old == BUTTON)
		XUngrabButton(dpy, 1, 0, ROOT);
	if (grabbed == BUTTON) {
		XGrabButton(dpy, 1, 0, ROOT, False,
				ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
					GrabModeSync, GrabModeAsync, None, None);
	}
}

Display *dpy;

class Handler;
Handler *handler = 0;

struct Handler {
	virtual void init() {
		grabber->grab(Grabber::BUTTON);
		XFlush(dpy);
	}
	virtual void press() {
	}
	virtual void release() {
		grabber->grab(Grabber::NONE);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		XTestFakeRelativeMotionEvent(dpy, 0, 0, 5);
		grabber->grab(Grabber::BUTTON);
		XFlush(dpy);
	}
};

bool handle(Glib::IOCondition) {
	while (XPending(dpy)) {
		XEvent ev;
		XNextEvent(dpy, &ev);
		if (ev.type == press) {
			XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
			printf("Press (Xi): %d (%d, %d, %d, %d, %d) at t = %ld\n",bev->button, bev->x, bev->y,
					bev->axis_data[0], bev->axis_data[1], bev->axis_data[2], bev->time);
			handler->press();
		}
		if (ev.type == release) {
			XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
			printf("Release (Xi): %d (%d, %d, %d, %d, %d)\n", bev->button, bev->x, bev->y,
					bev->axis_data[0], bev->axis_data[1], bev->axis_data[2]);
			handler->release();
		}
	}
	return true;
}

int main(int argc, char **argv) {
	Gtk::Main kit(argc, argv);

	dpy = XOpenDisplay(NULL);
	if (!dpy)
		exit(EXIT_FAILURE);

	grabber = new Grabber;
	grabber->grab(Grabber::BUTTON);

	handler = new Handler;
	handler->init();
	Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(dpy), Glib::IO_IN);
	io->connect(sigc::ptr_fun(&handle));
	io->attach();
	Gtk::Main::run();
	delete grabber;
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
