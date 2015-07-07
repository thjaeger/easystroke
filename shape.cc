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
#include <gdk/gdk.h>
#include "prefdb.h"
#include "shape.h"
#include "main.h"

#include <X11/extensions/shape.h>

Shape::Shape() {
	int w = gdk_screen_width();
	int h = gdk_screen_height();
	Gdk::Color col = prefs.color.get().color;
	unsigned long bg = ((col.get_red()/257)<<16) + ((col.get_green()/257)<<8) + col.get_blue()/257;
	win = XCreateSimpleWindow(dpy, ROOT, 0, 0, w, h, 0, CopyFromParent, bg);
	XSetWindowAttributes attr;
	attr.override_redirect = True;
	XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &attr);

	clear();
}

void Shape::draw(Point p, Point q) {
	int px = (int)p.x, py = (int)p.y, qx = (int)q.x, qy = (int)q.y;
	int width = prefs.trace_width.get();
	int x = (MIN(px, qx) - width);
	int y = (MIN(py, qy) - width);
	int w = (ABS(px - qx) + 2*width);
	int h = (ABS(py - qy) + 2*width);
	Pixmap pm = XCreatePixmap(dpy, DefaultRootWindow(dpy), w, h, 1);

	XGCValues gcv;
	gcv.foreground = 0;
	gcv.line_width = width;
	gcv.cap_style = CapRound;
	GC gc = XCreateGC(dpy, pm, GCCapStyle | GCForeground | GCLineWidth, &gcv);
	XFillRectangle(dpy, pm, gc, 0, 0, w, h);
	XSetForeground(dpy, gc, 1);
	XDrawLine(dpy, pm, gc, px-x, py-y, qx-x, qy-y);
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
	XShapeCombineRectangles(dpy, win, ShapeBounding, 0, 0, nullptr, 0, ShapeSet, YXBanded);
}

Shape::~Shape() {
	XDestroyWindow(dpy, win);
}
