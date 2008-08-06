/*
 * Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <map>
#include <X11/Xproto.h>

template <class T> class Stack {
	std::map<T, unsigned long> map1;
	std::map<unsigned long, T> map2;
	unsigned long n;
public:
	Stack() : n(0) {}
	void push(T t) {
		if (map1.count(t))
			return;
		map1[t] = n;
		map2[n] = t;
		n++;
	}

	void erase(T t) {
		if (!map1.count(t))
			return;
		map2.erase(map1[t]);
		map1.erase(t);
	}

	bool empty() {
		return map1.empty();
	}

	T pop() {
		typename std::map<T, unsigned long>::reverse_iterator i = map1.rbegin();
		T t = i->first;
		map2.erase(i->second);
		map1.erase(t);
		return t;
	}
};

Stack<Window> minimized;

Display *dpy;
#define ROOT (DefaultRootWindow(dpy))

Atom ATOM, WM_STATE, _NET_ACTIVE_WINDOW, _NET_WM_STATE, _NET_WM_STATE_HIDDEN;

int xErrorHandler(Display *dpy, XErrorEvent *e) {
	if (e->error_code == BadWindow && e->request_code == X_ChangeWindowAttributes)
		return 0;
	char text[64];
	XGetErrorText(dpy, e->error_code, text, sizeof text);
	char msg[16];
	snprintf(msg, sizeof msg, "%d", e->request_code);
	char def[32];
	snprintf(def, sizeof def, "request_code=%d", e->request_code);
	char dbtext[128];
	XGetErrorDatabaseText(dpy, "XRequest", msg,
			def, dbtext, sizeof dbtext);
	printf("XError: %s: %s\n", text, dbtext);
	return 0;
}

void create(Window w) {
	XSelectInput(dpy, w, PropertyChangeMask);
}

bool has_wm_state(Window w) {
	Atom actual_type_return;
	int actual_format_return;
	unsigned long nitems_return;
	unsigned long bytes_after_return;
	unsigned char *prop_return;
	if (Success != XGetWindowProperty(dpy, w, WM_STATE, 0, 2, False,
				AnyPropertyType, &actual_type_return,
				&actual_format_return, &nitems_return,
				&bytes_after_return, &prop_return))
		return false;
	XFree(prop_return);
	return nitems_return;
}

void init(Window w, int depth = 0) {
	depth++;
	if (depth > 2)
		return;
	unsigned int n;
	Window dummyw1, dummyw2, *ch;
	XQueryTree(dpy, w, &dummyw1, &dummyw2, &ch, &n);
	for (unsigned int i = 0; i != n; i++) {
		if (!has_wm_state(ch[i]))
			init(ch[i], depth);
		else
			create(ch[i]);
	}
	XFree(ch);
}

void activate(Window w, Time t) {
	XClientMessageEvent ev;
	ev.type = ClientMessage;
	ev.window = w;
	ev.message_type = _NET_ACTIVE_WINDOW;
	ev.format = 32;
	ev.data.l[0] = 0; // 1 app, 2 pager
	ev.data.l[1] = t;
	ev.data.l[2] = 0;
	ev.data.l[3] = 0;
	ev.data.l[4] = 0;
	XSendEvent(dpy, ROOT, False, SubstructureNotifyMask | SubstructureRedirectMask, (XEvent *)&ev);
}

bool has_atom(Window w, Atom prop, Atom value) {
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop_return = NULL;

	if (XGetWindowProperty(dpy, w, prop, 0, sizeof(Atom), False, ATOM, &actual_type, &actual_format,
				&nitems, &bytes_after, &prop_return) != Success)
		return None;
	if (!prop_return)
		return None;
	Atom *atoms = (Atom *)prop_return;
	bool ans = false;
	for (unsigned long i = 0; i < nitems; i++)
		if (atoms[i] == value)
			ans = true;
	XFree(prop_return);
	return ans;
}

int main(int argc, char *argv[]) {
	dpy = XOpenDisplay(NULL);
	ATOM = XInternAtom(dpy, "ATOM", False);
	WM_STATE = XInternAtom(dpy, "WM_STATE", False);
	_NET_ACTIVE_WINDOW = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	_NET_WM_STATE = XInternAtom(dpy, "_NET_WM_STATE", False);
	_NET_WM_STATE_HIDDEN = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);

	XGrabKey(dpy, XKeysymToKeycode(dpy,XK_F9), Mod1Mask|Mod4Mask, ROOT, False, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy,XK_F9), Mod1Mask|Mod2Mask|Mod4Mask, ROOT, False, GrabModeAsync, GrabModeAsync);
	XSetErrorHandler(xErrorHandler);
	XSelectInput(dpy, ROOT, SubstructureNotifyMask);
	init(ROOT);

	while (1) {
		XEvent ev;
		XNextEvent(dpy, &ev);
		switch (ev.type) {
			case CreateNotify:
				create(ev.xcreatewindow.window);
				break;

			case DestroyNotify:
				minimized.erase(ev.xdestroywindow.window);
				break;

			case PropertyNotify:
				if (ev.xproperty.atom != _NET_WM_STATE)
					break;
				if (ev.xproperty.state == PropertyDelete) {
					minimized.erase(ev.xproperty.window);
					break;
				}
				if (has_atom(ev.xproperty.window, _NET_WM_STATE, _NET_WM_STATE_HIDDEN))
					minimized.push(ev.xproperty.window);
				else
					minimized.erase(ev.xproperty.window);
				break;

			case KeyPress:
				if (minimized.empty())
					break;
				activate(minimized.pop(), ev.xkey.time);
				break;
		}
	}
}
