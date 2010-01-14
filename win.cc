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
	if (trigger)
		str = Glib::ustring::compose("%1\xE2\x86\x92", trigger);
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

Source<bool> disabled;

class MenuCheck : private Base {
	IO<bool> &io;
	Gtk::CheckMenuItem *check;
	virtual void notify() { check->set_active(io.get()); }
	void on_changed() {
		bool b = check->get_active();
		if (b == io.get()) return;
		io.set(b);
	}
public:
	MenuCheck(IO<bool> &io_, Gtk::CheckMenuItem *check_) : io(io_), check(check_) {
		io.connect(this);
		notify();
		check->signal_toggled().connect(sigc::mem_fun(*this, &MenuCheck::on_changed));
	}
};

extern void quit();

Win::Win() : actions(new Actions), prefs_tab(new Prefs), stats(new Stats) {
	show_hide_icon();
	prefs.tray_icon.connect(new Notifier(sigc::mem_fun(*this, &Win::show_hide_icon)));
	disabled.connect(new Notifier(sigc::mem_fun(*this, &Win::timeout)));

	WIDGET(Gtk::CheckMenuItem, menu_disabled, _("D_isabled"), true);
	menu.append(menu_disabled);
	new MenuCheck(disabled, &menu_disabled);

	WIDGET(Gtk::ImageMenuItem, menu_about, Gtk::Stock::ABOUT);
	menu.append(menu_about);
	menu_about.signal_activate().connect(sigc::mem_fun(*this, &Win::on_about));

	WIDGET(Gtk::SeparatorMenuItem, menu_sep);
	menu.append(menu_sep);

	WIDGET(Gtk::ImageMenuItem, menu_quit, Gtk::Stock::QUIT);
	menu.append(menu_quit);
	menu_quit.signal_activate().connect(sigc::ptr_fun(&quit));

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

extern void icon_warning();

static gboolean icon_clicked(GtkStatusIcon *status_icon, GdkEventButton *event, gpointer) {
	if (event->button == 2)
		disabled.set(!disabled.get());
	return TRUE;
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
		if (gtk_major_version >= 2 && gtk_minor_version >= 15)
			g_signal_connect(icon->gobj(), "button-release-event", G_CALLBACK(icon_clicked), NULL);
	} else {
		if (icon)
			icon.reset();
		icon_warning();
	}
}

void Win::show_popup(guint button, guint32 activate_time) {
	if (icon)
		icon->popup_menu_at_position(menu, button, activate_time);
}

extern const char *version_string;
void Win::on_about() {
	Gtk::AboutDialog *about;
	widgets->get_widget("aboutdialog", about);
	about->set_logo(Stroke::trefoil()->draw(96, 4.0));
	about->set_version(version_string);
	about->set_program_name("easystroke\n");
	about->show();
	about->run();
	about->hide();
}

void Win::show_hide() {
	if (win->is_mapped())
		win->hide();
	else
		win->show();
}

bool Win::on_icon_size_changed(int size) {
	icon_pb[0] = Stroke::trefoil()->draw(size);
	icon_pb[1] = Stroke::trefoil()->draw(size);
	icon_pb[1]->saturate_and_pixelate(icon_pb[1], 0.0, true);
	if (icon)
		icon->set(icon_pb[disabled.get() ? 1 : 0]);
	return true;
}

void Win::timeout() {
	if (icon)
		icon->set(icon_pb[disabled.get() ? 1 : 0]);
}

void Win::set_icon(RStroke stroke, bool invert) {
	if (!icon)
		return;
	icon->set(stroke->draw(icon->get_size(), 2.0, invert));
	set_timeout(10000);
}

void error_dialog(const Glib::ustring &text) {
	Gtk::MessageDialog dialog(win->get_window(), text, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
	dialog.show();
	dialog.run();
}

Glib::ustring app_name_hr(Glib::ustring src) {
	return src == "" ? _("(window manager frame)") : src;
}
