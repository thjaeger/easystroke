#include "prefdb.h"
#include "composite.h"
#include <gdkmm.h>

double red, green, blue, alpha, width;
std::list<Trace::Point> points;

Popup::Popup(int x1, int y1, int x2, int y2) : Gtk::Window(Gtk::WINDOW_POPUP), rect(x1, y1, x2-x1, y2-y1) {
	if (!is_composited())
		throw std::runtime_error("'composite' not available");

	Glib::RefPtr<Gdk::Visual> visual = get_screen()->get_rgba_visual();
	gtk_widget_set_visual(Widget::gobj(), visual->gobj());
	gtk_widget_set_app_paintable (Widget::gobj(), TRUE);
	signal_draw().connect(sigc::mem_fun(*this, &Popup::on_draw));
	realize();
	move(x1, y1);
	resize(x2-x1, y2-y1);
	get_window()->input_shape_combine_region(Cairo::Region::create(), 0, 0);
	// tell compiz to leave this window the hell alone
	get_window()->set_type_hint(Gdk::WINDOW_TYPE_HINT_DESKTOP);
}

void Popup::invalidate(int x1, int y1, int x2, int y2) {
    if (get_mapped()) {
        Gdk::Rectangle inv(x1 - rect.get_x(), y1 - rect.get_y(), x2 - x1, y2 - y1);
        get_window()->invalidate_rect(inv, false);
    } else {
        show();
    }
}

Composite::Composite() {
#define N 128
	int w = gdk_screen_width();
	int h = gdk_screen_height();
	num_x = (w  - 1)/N + 1;
	num_y = (h - 1)/N + 1;
	pieces = new Popup**[num_x];
	for (int i = 0; i < num_x; i++) {
        pieces[i] = new Popup *[num_y];
        for (int j = 0; j < num_y; j++) {
            pieces[i][j] = new Popup(i * N, j * N, MIN((i + 1) * N, w), MIN((j + 1) * N, h));
        }
    }
}

void Composite::draw(Point p, Point q) {
	if (points.empty()) {
		points.push_back(p);
	}
	points.push_back(q);
	int x1 = (int)(p.x < q.x ? p.x : q.x);
	int x2 = (int)(p.x < q.x ? q.x : p.x);
	int y1 = (int)(p.y < q.y ? p.y : q.y);
	int y2 = (int)(p.y < q.y ? q.y : p.y);
	int bw = (int)(width/2.0) + 2;
	x1 -= bw; y1 -= bw;
	x2 += bw; y2 += bw;
	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)

		y1 = 0;
	for (int i = x1/N; i<num_x && i<=x2/N; i++)
		for (int j = y1/N; j<num_y && j<=y2/N; j++)
			pieces[i][j]->invalidate(x1, y1, x2, y2);
}

void Composite::start_() {
	RGBA rgba = prefs.color;
	red = rgba.color.get_red_p();
	green = rgba.color.get_green_p();
	blue = rgba.color.get_blue_p();
	alpha = ((double)rgba.alpha)/65535.0;
	width = prefs.trace_width;
}

void Popup::draw_line(Cairo::RefPtr<Cairo::Context> ctx) {
	if (points.empty())
		return;
	auto i = points.begin();
	ctx->move_to (i->x, i->y);
	for (; i != points.end(); i++)
		ctx->line_to (i->x, i->y);
	ctx->set_source_rgba((red+0.5)/2.0, (green+0.5)/2.0, (blue+0.5)/2.0, alpha/2.0);
	ctx->set_line_width(width+1.0);
	ctx->set_line_cap(Cairo::LINE_CAP_ROUND);
	ctx->set_line_join(Cairo::LINE_JOIN_ROUND);
	ctx->stroke_preserve();

	ctx->set_source_rgba(red, green, blue, alpha);
	ctx->set_line_width(width*0.7);
	ctx->stroke();
}

bool Popup::on_draw(const ::Cairo::RefPtr< ::Cairo::Context>& ctx) {
	ctx->set_operator(Cairo::OPERATOR_SOURCE);
	ctx->set_source_rgba(0.0, 0.0, 0.0, 0.0);
	ctx->paint();

	ctx->translate(-rect.get_x(), -rect.get_y());
	draw_line(ctx);

	return false;
}

void Composite::end_() {
	points.clear();
	for (int i = 0; i < num_x; i++)
		for (int j = 0; j < num_y; j++)
			pieces[i][j]->hide();
}

Composite::~Composite() {
	for (int i = 0; i < num_x; i++) {
		for (int j = 0; j < num_y; j++)
			delete pieces[i][j];
		delete[] pieces[i];
	}
	delete[] pieces;
}
