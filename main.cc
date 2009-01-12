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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XTest.h>

Display *dpy;
Window root;

int press, release;

XDevice *dev;
XEventClass events[3];
int all_events_n;
int button_events_n;

void handle() {
	XEvent ev;
	XNextEvent(dpy, &ev);
	if (ev.type != release)
		return;
	static int i = 1;
	printf("Release %d\n", i++);
	XGrabDevice(dpy, dev, root, False, all_events_n, events, GrabModeAsync, GrabModeAsync, CurrentTime);
	XAllowEvents(dpy, ReplayPointer, CurrentTime);
	XTestFakeRelativeMotionEvent(dpy, 0, 0, 20);
	XUngrabDevice(dpy, dev, CurrentTime);
}

void init_xi() {
	int n;
	XDeviceInfo *devs = XListInputDevices(dpy, &n);
	if (!devs)
		exit(EXIT_FAILURE);
	for (int i = 0; i < n; i++) {
		if (strcmp(devs[i].name, "stylus"))
//		if (strcmp(devs[i].name, "TPPS/2 IBM TrackPoint"))
			continue;
		dev = XOpenDevice(dpy, devs[i].id);
		break;
	}
	if (!dev)
		exit(EXIT_FAILURE);
	XFreeDeviceList(devs);

	int dummy;
	DeviceButtonPress(dev, press, events[0]);
	DeviceButtonRelease(dev, release, events[1]);
	button_events_n = 2;
	DeviceMotionNotify(dev, dummy, events[2]);
	all_events_n = 3;
}

int main(int argc, char **argv) {
	dpy = XOpenDisplay(NULL);
	if (!dpy)
		exit(EXIT_FAILURE);
	root = DefaultRootWindow(dpy);

	init_xi();
	XGrabDeviceButton(dpy, dev, 1, 0, NULL, root, False, button_events_n, events, GrabModeAsync, GrabModeAsync);
	XGrabButton(dpy, 1, 0, root, False, ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
	while (true)
		handle();
}
