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
#include "prefs.h"
#include "stats.h"
#include "win.h"
#include "main.h"
#include "grabber.h"
#include <glibmm/i18n.h>

Glib::RefPtr<Gtk::Builder> widgets;

void Stroke::draw(Cairo::RefPtr<Cairo::Surface> surface, int x, int y, int w, int h, bool big) const {
	const Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create (surface);
	x+=2; y+=2; w -= 4; h -= 4;
	ctx->save();
	ctx->translate(x,y);
	ctx->scale(w,h);
	ctx->set_line_width((big ? 8.0 : 4.0)/(w+h));
	if (size()) {
		ctx->set_line_cap(Cairo::LINE_CAP_ROUND);
		int n = points.size();
		float lambda = sqrt(3)-2.0;
		float sum = lambda / (1 - lambda);
		std::vector<Point> y(n);
		y[0].x = sum * points[0].x;
		y[0].y = sum * points[0].y;
		for (int j = 0; j < n-1; j++) {
			y[j+1].x = lambda * (y[j].x + points[j].x);
			y[j+1].y = lambda * (y[j].y + points[j].y);
		}
		std::vector<Point> z(n);
		z[n-1].x = -1.0 * sum * points[n-1].x;
		z[n-1].y = -1.0 * sum * points[n-1].y;
		for (int j = n-1; j > 0; j--) {
			z[j-1].x = lambda * (z[j].x - points[j].x);
			z[j-1].y = lambda * (z[j].y - points[j].y);
		}
		for (int j = 0; j < n-1; j++) {
			// j -> j+1
			ctx->set_source_rgba(0, points[j].time, 1-points[j].time, 1);
			ctx->move_to(points[j].x, points[j].y);
			ctx->curve_to(
					points[j].x + y[j].x + z[j].x,
					points[j].y + y[j].y + z[j].y,
					points[j+1].x - y[j+1].x - z[j+1].x,
					points[j+1].y - y[j+1].y - z[j+1].y,
					points[j+1].x,
					points[j+1].y);
			ctx->stroke();
		}
	} else if (!button) {
		ctx->set_source_rgba(0, 0, 1, 1);
		ctx->move_to(0.33, 0.33);
		ctx->line_to(0.67, 0.67);
		ctx->move_to(0.33, 0.67);
		ctx->line_to(0.67, 0.33);
		ctx->stroke();
	}
	ctx->restore();
	Glib::ustring str;
	if (trigger)
		str = Glib::ustring::compose("%1\xE2\x86\x92", trigger);
	if (timeout)
		str += "x";
	if (button)
		str += Glib::ustring::compose("%1", button);
	if (str == "")
		return;
	ctx->set_source_rgba(1, 0, 0, 0.8);
	ctx->set_font_size(h*0.5);
	Cairo::TextExtents te;
	ctx->get_text_extents(str, te);
	ctx->move_to(x+w/2 - te.x_bearing - te.width/2, y+h/2 - te.y_bearing - te.height/2);
	ctx->show_text(str);
}

void Stroke::draw_svg(std::string filename) const {
	const int S = 32;
	const int B = 1;
	Cairo::RefPtr<Cairo::SvgSurface> s = Cairo::SvgSurface::create(filename, S, S);
	draw(s, B, B, S-2*B, S-2*B, false);
}


Glib::RefPtr<Gdk::Pixbuf> Stroke::draw_(int size, bool big) const {
	Glib::RefPtr<Gdk::Pixbuf> pb = drawEmpty_(size);
	int w = size;
	int h = size;
	int stride = pb->get_rowstride();
	guint8 *row = pb->get_pixels();
	// This is all pretty messed up
	// http://www.archivum.info/gtkmm-list@gnome.org/2007-05/msg00112.html
	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create(row, Cairo::FORMAT_ARGB32, w, h, stride);
	draw(surface, 0, 0, pb->get_width(), size, big);
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

extern const char *gui_buffer;

int run_dialog(const char *str) {
	Glib::RefPtr<Gtk::Builder> xml = Gtk::Builder::create_from_string(gui_buffer);
	Gtk::Dialog *dialog;
	xml->get_widget(str, dialog);
	int response = dialog->run();
	dialog->hide();
	return response;
}

Win::Win() {
	widgets = Gtk::Builder::create_from_string(gui_buffer),
	actions = new Actions;
	prefs_tab = new Prefs;
	stats = new Stats;

	show_hide_icon();
	prefs.tray_icon.connect(new Notifier(sigc::mem_fun(*this, &Win::show_hide_icon)));

	WIDGET(Gtk::CheckMenuItem, menu_disabled, _("D_isabled"), true);
	this->menu_disabled = &menu_disabled;
	menu.append(menu_disabled);
	menu_disabled.signal_toggled().connect(sigc::mem_fun(*grabber, &Grabber::toggle_disabled));
	WIDGET(Gtk::ImageMenuItem, menu_quit, Gtk::Stock::QUIT);
	menu.append(menu_quit);
	menu_quit.signal_activate().connect(sigc::ptr_fun(&Gtk::Main::quit));
	menu.show_all();

	widgets->get_widget("main", win);
	RStroke trefoil = Stroke::trefoil();
	std::vector<Glib::RefPtr<Gdk::Pixbuf> > icons;
	icons.push_back(trefoil->draw(24));
	icons.push_back(trefoil->draw(64));
	win->set_icon_list(icons);

	Gtk::Button* button_hide[4];
	widgets->get_widget("button_hide1", button_hide[0]);
	widgets->get_widget("button_hide2", button_hide[1]);
	widgets->get_widget("button_hide3", button_hide[2]);
	widgets->get_widget("button_hide4", button_hide[3]);
	for (int i = 0; i < 4; i++)
		button_hide[i]->signal_clicked().connect(sigc::mem_fun(win, &Gtk::Window::hide));
}

Win::~Win() {
	delete actions;
	delete prefs_tab;
	delete stats;
}

void Win::toggle_disabled() {
	bool disabled = menu_disabled->get_active();
	menu_disabled->set_active(!disabled);
}

void Win::show_hide_icon() {
	bool show = prefs.tray_icon.get();
	if (show) {
		if (icon)
			return;
		icon = Gtk::StatusIcon::create("");
		icon->signal_size_changed().connect(sigc::mem_fun(*this, &Win::on_icon_size_changed));
		icon->signal_activate().connect(sigc::mem_fun(*this, &Win::show_hide));
		icon->signal_popup_menu().connect(sigc::mem_fun(*this, &Win::show_popup));
	} else {
		if (!icon)
			return;
		icon.reset();
	}
}

void Win::show_popup(guint button, guint32 activate_time) {
	icon->popup_menu_at_position(menu, button, activate_time);
}

void Win::show_hide() {
	if (win->is_mapped())
		win->hide();
	else
		win->show();
}

void composite_stock(Gtk::Widget *widget, Glib::RefPtr<Gdk::Pixbuf> dest, Glib::ustring name, double scale) {
		Glib::RefPtr<Gdk::Pixbuf> pb = widget->render_icon(Gtk::StockID(name), Gtk::ICON_SIZE_MENU);
		int w = (int)(dest->get_width() * scale);
		int h = (int)(dest->get_height() * scale);
		int x = (int)(dest->get_width() - w);
		int y = 0;
		double scale_x = (double)w/(double)(pb->get_width());
		double scale_y = (double)h/(double)(pb->get_height());
		pb->composite(dest, x, y, w, h, x, y, scale_x, scale_y, Gdk::INTERP_HYPER, 255);
}

bool Win::on_icon_size_changed(int size) {
	icon_pb[0] = Stroke::trefoil()->draw(size);
	icon_pb[1] = Stroke::trefoil()->draw(size);
	icon_pb[2] = Stroke::trefoil()->draw(size);
	composite_stock(win, icon_pb[1], "gtk-yes", 0.6);
	composite_stock(win, icon_pb[2], "gtk-no", 0.5);
	icon->set(icon_pb[0]);
	return true;
}

void Win::timeout() {
	icon->set(icon_pb[0]);
}

void Win::show_success(bool good) {
	if (!icon)
		return;
	icon->set(icon_pb[good ? 1 : 2]);
	set_timeout(2000);
}

FormatLabel::FormatLabel(Glib::RefPtr<Gtk::Builder> builder, Glib::ustring name, ...) {
	builder->get_widget(name, label);
	oldstring = label->get_label();
	char newstring[256];
	va_list argp;
	va_start(argp, name);
	vsnprintf(newstring, 255, oldstring.c_str(), argp);
	va_end(argp);
	label->set_label(newstring);
	label->set_use_markup();
}

FormatLabel::~FormatLabel() {
	label->set_text(oldstring);
}

ErrorDialog::ErrorDialog(const Glib::ustring &text) :
		MessageDialog(win->get_window(), text, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true)
	{ show(); }
