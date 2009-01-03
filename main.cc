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

Display *dpy;
Window root;

int press, release;

XDevice *dev;
XEventClass events[4];
int all_events_n;
int button_events_n;

void grab_xi_devs(bool grab) {
	if (grab)
		XGrabDevice(dpy, dev, root, False, all_events_n, events, GrabModeAsync, GrabModeAsync, CurrentTime);
	else
		XUngrabDevice(dpy, dev, CurrentTime);
}

void grab_core(bool grab) {
	if (grab)
		XGrabButton(dpy, 1, 0, root, False,
				ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
					GrabModeSync, GrabModeAsync, None, None);
	else
		XUngrabButton(dpy, 1, 0, root);
}

bool handle(Glib::IOCondition) {
	while (XPending(dpy)) {
		XEvent ev;
		XNextEvent(dpy, &ev);
		if (ev.type == press) {
			printf("Press (Xi)\n");
		}
		if (ev.type == release) {
			printf("Release (Xi)\n");
			grab_xi_devs(true);
			grab_core(false);
			XAllowEvents(dpy, ReplayPointer, CurrentTime);
			XTestFakeRelativeMotionEvent(dpy, 0, 0, 5);
			grab_xi_devs(false);
			grab_core(true);
		}
	}
	return true;
}

void init_xi() {
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
}

int main(int argc, char **argv) {
	Gtk::Main kit(argc, argv);
	dpy = XOpenDisplay(NULL);
	if (!dpy)
		exit(EXIT_FAILURE);
	root = DefaultRootWindow(dpy);

	init_xi();
	XGrabDeviceButton(dpy, dev, 1, 0, NULL, root, False, button_events_n, events, GrabModeAsync, GrabModeAsync);
	grab_core(true);
	XFlush(dpy);

	Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(dpy), Glib::IO_IN);
	io->connect(sigc::ptr_fun(&handle));
	io->attach();
	Gtk::Main::run();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
