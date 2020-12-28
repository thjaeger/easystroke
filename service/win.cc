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
#include "actions.h"
#include "win.h"
#include "main.h"
#include <glibmm/i18n.h>

Glib::RefPtr<Gtk::Builder> widgets;

void Stroke::draw(Cairo::RefPtr<Cairo::Surface> surface, int x, int y, int w, int h, double width, bool inv) const {
	const Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create (surface);
	x += width; y += width; w -= 2*width; h -= 2*width;
	ctx->save();
	ctx->translate(x,y);
	ctx->scale(w,h);
	ctx->set_line_width(2.0*width/(w+h));
	if (size()) {
		ctx->set_line_cap(Cairo::LINE_CAP_ROUND);
		int n = size();
		float lambda = sqrt(3)-2.0;
		float sum = lambda / (1 - lambda);
		std::vector<Point> y(n);
		y[0] = points(0) * sum;
		for (int j = 0; j < n-1; j++)
			y[j+1] = (y[j] + points(j)) * lambda;
		std::vector<Point> z(n);
		z[n-1] = points(n-1) * (-sum);
		for (int j = n-1; j > 0; j--)
			z[j-1] = (z[j] - points(j)) * lambda;
		for (int j = 0; j < n-1; j++) {
			// j -> j+1
			if (inv)
				ctx->set_source_rgba(time(j), 0.0, 1.0-time(j), 1.0);
			else
				ctx->set_source_rgba(0.0, time(j), 1.0-time(j), 1.0);
			Point p[4];
			p[0] = points(j);
			p[3] = points(j+1);
			p[1] = p[0] + y[j] + z[j];
			p[2] = p[3] - y[j+1] - z[j+1];
			ctx->move_to(p[0].x, p[0].y);
			ctx->curve_to(p[1].x, p[1].y, p[2].x, p[2].y, p[3].x, p[3].y);
			ctx->stroke();
		}
	} else if (!button) {
		if (inv)
			ctx->set_source_rgba(1.0, 1.0, 0.0, 1.0);
		else
			ctx->set_source_rgba(0.0, 0.0, 1.0, 1.0);
		ctx->move_to(0.33, 0.33);
		ctx->line_to(0.67, 0.67);
		ctx->move_to(0.33, 0.67);
		ctx->line_to(0.67, 0.33);
		ctx->stroke();
	}
	ctx->restore();
	Glib::ustring str;
	if (modifiers != AnyModifier) {
		str = Gtk::AccelGroup::get_label(0, (Gdk::ModifierType)modifiers);
		if (str == "")
			str = "<>";
		else
			str = "<" + str.substr(0, str.size()-1) + ">";
	}
	if (trigger)
		str += Glib::ustring::compose("%1\xE2\x86\x92", trigger);
	if (timeout)
		str += "x";
	if (button)
		str += Glib::ustring::compose("%1", button);
	if (str == "")
		return;
	if (inv)
		ctx->set_source_rgba(0.0, 1.0, 1.0, 0.8);
	else
		ctx->set_source_rgba(1.0, 0.0, 0.0, 0.8);
	float font_size = h*0.5;
	Cairo::TextExtents te;
	for (;;) {
		ctx->set_font_size(font_size);
		ctx->get_text_extents(str, te);
		if (te.width < w)
			break;
		font_size *= 0.9;
	}
	ctx->move_to(x+w/2 - te.x_bearing - te.width/2, y+h/2 - te.y_bearing - te.height/2);
	ctx->show_text(str);
}

void Stroke::draw_svg(std::string filename) const {
	const int S = 32;
	const int B = 1;
	Cairo::RefPtr<Cairo::SvgSurface> s = Cairo::SvgSurface::create(filename, S, S);
	draw(s, B, B, S-2*B, S-2*B);
}


Glib::RefPtr<Gdk::Pixbuf> Stroke::draw_(int size, double width, bool inv) const {
	Glib::RefPtr<Gdk::Pixbuf> pb = drawEmpty_(size);
	int w = size;
	int h = size;
	int stride = pb->get_rowstride();
	guint8 *row = pb->get_pixels();
	// This is all pretty messed up
	// http://www.archivum.info/gtkmm-list@gnome.org/2007-05/msg00112.html
	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create(row, Cairo::FORMAT_ARGB32, w, h, stride);
	draw(surface, 0, 0, pb->get_width(), size, width, inv);
	for (int i = 0; i < w; i++) {
		guint8 *px = row;
		for (int j = 0; j < h; j++) {
			guint8 a = px[3];
			guint8 r = px[2];
			guint8 g = px[1];
			guint8 b = px[0];
			if (a) {
				px[0] = ((((guint)r) << 8) - r) / a;
				px[1] = ((((guint)g) << 8) - g) / a;
				px[2] = ((((guint)b) << 8) - b) / a;
			}
			px += 4;
		}
		row += stride;
	}
	return pb;
}


Glib::RefPtr<Gdk::Pixbuf> Stroke::drawEmpty_(int size) {
	Glib::RefPtr<Gdk::Pixbuf> pb = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB,true,8,size,size);
	pb->fill(0x00000000);
	return pb;
}

Source<bool> disabled(false);

extern const char *version_string;
