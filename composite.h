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
#ifndef __COMPOSITE_H__
#define __COMPOSITE_H__
#include <gtkmm.h>
#include "trace.h"
#include "main.h"
#include <list>

class Composite;

class Popup : public Gtk::Window {
	bool on_expose(GdkEventExpose* event);
	void draw_line(Cairo::RefPtr<Cairo::Context> ctx);
	Gdk::Rectangle rect;
	Composite *composite;
public:
	Popup(Composite *comp,int x1, int y1, int x2, int y2);
	void invalidate(int x1, int y1, int x2, int y2);
};

class Composite : public Trace {
	int num_x, num_y;
	Popup ***pieces;
	virtual void draw(Point p, Point q);
	virtual void start_();
	virtual void end_();
public:
	std::list<Trace::Point> points;
	Composite();
	virtual ~Composite();
};

#endif
