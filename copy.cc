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
#include "copy.h"
#include <X11/Xutil.h>

Copy::Copy() {
	int screen = DefaultScreen(dpy);
	Window root = RootWindow(dpy, screen);
	int w = DisplayWidth(dpy, screen);
	int h = DisplayHeight(dpy, screen);
	win = XCreateSimpleWindow(dpy, root, 0, 0, w, h, 0, CopyFromParent, CopyFromParent);
	XSetWindowAttributes attr;
	attr.override_redirect = True;
	attr.save_under = True;
	attr.background_pixmap = None;
	XChangeWindowAttributes(dpy, win, CWOverrideRedirect | CWSaveUnder | CWBackPixmap, &attr);

	XGCValues gcv;
	gcv.foreground = TRACE_COLOR;
	gcv.line_width = WIDTH;
	gc = XCreateGC(dpy, win, GCForeground | GCLineWidth, &gcv);
}
