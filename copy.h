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
#ifndef __COPY_H__
#define __COPY_H__
#include "trace.h"
#include "main.h"

class Copy : public Trace {
	Window win;
	GC gc;
private:
	virtual void draw(Point p, Point q) { XDrawLine(dpy, win, gc, p.x, p.y, q.x, q.y); }
	virtual void start_() { XMapRaised(dpy, win); }
	virtual void end_() { XUnmapWindow(dpy, win); }
public:
	Copy();
	virtual ~Copy() { XFreeGC(dpy, gc); XDestroyWindow(dpy, win); }
};

#endif
