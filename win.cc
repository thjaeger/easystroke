#include "actions.h"
#include "prefs.h"
#include "stats.h"
#include "win.h"

void Stroke::draw(Cairo::RefPtr<Cairo::Surface> surface, int x, int y, int w, int h, bool invert) const {
	const Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create (surface);
	ctx->set_line_width(2);
	x++; y++; w -= 2; h -= 2;
	if (size()) {
		bool first = true;
		for (std::vector<Point>::const_iterator j = points.begin(); j!=points.end();j++) {
			if (first) {
				ctx->move_to(w*j->x+x,h*j->y+y);
				first = false;
			}
			ctx->line_to(w*j->x+x,h*j->y+y);
			if (invert)
				ctx->set_source_rgba(1-j->time, j->time, 0, 1);
			else
				ctx->set_source_rgba(0, j->time, 1-j->time, 1);
			ctx->stroke();
			ctx->move_to(w*j->x+x,h*j->y+y);
		}
	} else if (!button) {
		if (invert)
			ctx->set_source_rgba(1, 0, 0, 1);
		else
			ctx->set_source_rgba(0, 0, 1, 1);
		ctx->move_to(x+w/3, y+h/3);
		ctx->line_to(x+2*w/3, y+2*h/3);
		ctx->move_to(x+w/3, y+2*h/3);
		ctx->line_to(x+2*w/3, y+h/3);
		ctx->stroke();
	}
	if (!button)
		return;
	if (invert)
		ctx->set_source_rgba(0, 0, 1, 0.8);
	else
		ctx->set_source_rgba(1, 0, 0, 0.8);
	Glib::ustring str = Glib::ustring::format(button);
	ctx->set_font_size(h*0.6);
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


Glib::RefPtr<Gdk::Pixbuf> Stroke::draw_(int size) const {
	Glib::RefPtr<Gdk::Pixbuf> pb = drawEmpty_(size);
	// This is all pretty messed up
	// http://www.archivum.info/gtkmm-list@gnome.org/2007-05/msg00112.html
	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create (pb->get_pixels(),
			Cairo::FORMAT_ARGB32, size, size, pb->get_rowstride());

	draw(surface, 0, 0, pb->get_width(), size);
	return pb;
}


Glib::RefPtr<Gdk::Pixbuf> Stroke::drawEmpty_(int size) {
	Glib::RefPtr<Gdk::Pixbuf> pb = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB,true,8,size,size);
	pb->fill(0xffffff00);
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

Win::Win() :
	widgets(Gtk::Builder::create_from_string(gui_buffer)),
	actions(new Actions(this)),
	prefs(new Prefs(this)),
	stats(new Stats(this)),
	icon_queue(sigc::mem_fun(*this, &Win::on_icon_changed))
{
	current_icon = Stroke::trefoil();
	icon = Gtk::StatusIcon::create("");
	icon->signal_size_changed().connect(sigc::mem_fun(*this, &Win::on_icon_size_changed));
	icon->signal_activate().connect(sigc::mem_fun(*this, &Win::on_icon_click));
	icon->signal_popup_menu().connect(sigc::mem_fun(*this, &Win::show_popup));

	quit.connect(sigc::ptr_fun(&Gtk::Main::quit));

	WIDGET(Gtk::ImageMenuItem, menu_quit, Gtk::Stock::QUIT);
	menu.append(menu_quit);
	menu_quit.signal_activate().connect(sigc::ptr_fun(&Gtk::Main::quit));
	menu.show_all();

	widgets->get_widget("main", win);

	Gtk::Button* button_hide[3];
	widgets->get_widget("button_hide1", button_hide[0]);
	widgets->get_widget("button_hide2", button_hide[1]);
	widgets->get_widget("button_hide3", button_hide[2]);
	for (int i = 0; i < 3; i++)
		button_hide[i]->signal_clicked().connect(sigc::mem_fun(win, &Gtk::Window::hide));
}

Win::~Win() {
	delete actions;
	delete prefs;
	delete stats;
}

void Win::show_popup(guint button, guint32 activate_time) {
	icon->popup_menu_at_position(menu, button, activate_time);
}

void Win::stroke_push(Ranking& r) {
	stats->stroke_push(r);
}

void Win::on_icon_changed(RStroke s) {
	current_icon = s;
	on_icon_size_changed(icon->get_size());
}

void Win::on_icon_click() {
	if (win->is_mapped())
		win->hide();
	else
		win->show();
}

bool Win::on_icon_size_changed(int size) {
	if (!current_icon)
		return true;
	icon->set(current_icon->draw(size));
	return true;
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
