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
#include "composite.h"
#include <gdkmm.h>

#define BRAVE 1

Composite::Composite() : Gtk::Window(Gtk::WINDOW_POPUP) {
	if (!is_composited())
		throw std::runtime_error("composite not available");
	Glib::RefPtr<Gdk::Screen> screen = Gdk::Display::get_default()->get_default_screen();

	set_position(Gtk::WIN_POS_CENTER_ALWAYS);
	set_default_size(screen->get_width(), screen->get_height());
	set_decorated(false);
	set_app_paintable(true);

	Glib::RefPtr<Gdk::Colormap> colormap = get_screen()->get_rgba_colormap();
	if (colormap)
		set_colormap(colormap);
	signal_expose_event().connect(sigc::mem_fun(*this, &Composite::on_expose));
	realize();
	get_window()->input_shape_combine_region(Gdk::Region(), 0, 0);
#if BRAVE
	show();
#endif
}

void Composite::draw(Point p, Point q) {
	if (!points.size())
		points.push_back(p);
	points.push_back(q);
	int x = p.x < q.x ? p.x : q.x;
	int w = abs(q.x - p.x);
	int y = p.y < q.y ? p.y : q.y;
	int h = abs(q.y - p.y);
	get_window()->invalidate_rect(Gdk::Rectangle(x-10, y-10, w+20, h+20), false);
}

void Composite::start_() {
	show();
}

void Composite::draw_line(Cairo::RefPtr<Cairo::Context> ctx) {
	if (!points.size())
		return;
	std::list<Point>::iterator i = points.begin();
	ctx->move_to (i->x, i->y);
	for (; i != points.end(); i++)
		ctx->line_to (i->x, i->y);
	ctx->set_source_rgba(0.6, 0.2, 0.2, 0.5);
	ctx->set_line_width(7);
	ctx->set_line_cap(Cairo::LINE_CAP_ROUND);
	ctx->stroke_preserve();

	ctx->set_source_rgba(1.0, 0.2, 0.2, 0.8);
	ctx->set_line_width(5);
	ctx->set_line_cap(Cairo::LINE_CAP_ROUND);
	ctx->stroke();

}

bool Composite::on_expose(GdkEventExpose* event) {
	Cairo::RefPtr<Cairo::Context> ctx = get_window()->create_cairo_context();
	ctx->set_operator(Cairo::OPERATOR_SOURCE);

	Gdk::Region region(event->region, true);
	Gdk::Cairo::add_region_to_path(ctx, region);
	ctx->clip();

	Gdk::Cairo::add_region_to_path(ctx, region);
	ctx->set_source_rgba(0.0, 0.0, 0.0, 0.0);
	ctx->fill();

	draw_line(ctx);

	return false;
}

void Composite::end_() {
	points.clear();
#if BRAVE
	gdk_window_invalidate_rect(GDK_WINDOW(get_window()->gobj()), 0, false);
#else
	hide();
#endif
}

Composite::~Composite() {}
