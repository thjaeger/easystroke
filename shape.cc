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
#include "prefdb.h"
#include "shape.h"
#include "main.h"

#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

Shape::Shape() {
	int screen = DefaultScreen(dpy);
	Window root = RootWindow(dpy, screen);
	int w = DisplayWidth(dpy, screen);
	int h = DisplayHeight(dpy, screen);
	unsigned long bg = prefs.color.get();
	win = XCreateSimpleWindow(dpy, root, 0, 0, w, h, 0, CopyFromParent, bg);
	XSetWindowAttributes attr;
	attr.override_redirect = True;
	XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &attr);

	clear();
}

#define min(x,y) ((x) > (y) ? (y) : (x))
#define abs(x) ((x) > 0 ? (x) : -(x))

void Shape::draw(Point p, Point q) {
	int x = min(p.x, q.x) - WIDTH;
	int y = min(p.y, q.y) - WIDTH;
	int w = abs(p.x - q.x) + 2*WIDTH;
	int h = abs(p.y - q.y) + 2*WIDTH;
	Pixmap pm = XCreatePixmap(dpy, DefaultRootWindow(dpy), w, h, 1);

	XGCValues gcv;
	gcv.foreground = 0;
	gcv.line_width = WIDTH;
	gcv.cap_style = CapRound;
	GC gc = XCreateGC(dpy, pm, GCCapStyle | GCForeground | GCLineWidth, &gcv);
	XFillRectangle(dpy, pm, gc, 0, 0, w, h);
	XSetForeground(dpy, gc, 1);
	XDrawLine(dpy, pm, gc, p.x-x, p.y-y, q.x-x, q.y-y);
	XFreeGC(dpy, gc);

	XShapeCombineMask(dpy, win, ShapeBounding, x, y, pm, ShapeUnion);
	XFreePixmap(dpy, pm);
}

void Shape::start_() {
	if (remove_timeout())
		clear();
	XMapRaised(dpy, win);
}

void Shape::end_() {
	XUnmapWindow(dpy, win);
	set_timeout(10);
}

void Shape::timeout() {
	clear();
	XFlush(dpy);
}

void Shape::clear() {
	XShapeCombineRectangles(dpy, win, ShapeBounding, 0, 0, NULL, 0, ShapeSet, YXBanded);
}

Shape::~Shape() {
	XDestroyWindow(dpy, win);
}
