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
#include "win.h"
#include "actiondb.h"
#include "main.h"
#include <iomanip>
#include <glibmm/i18n.h>
#include <sys/time.h>

Stats::Stats() {
	Gtk::Button *button_matrix;
	widgets->get_widget("button_matrix", button_matrix);
	widgets->get_widget("treeview_recent", recent_view);
	widgets->get_widget("treeview_ranking", ranking_view);

	recent_store = Gtk::ListStore::create(cols);
	recent_view->set_model(recent_store);
	recent_view->append_column(_("Stroke"), cols.stroke);
	recent_view->append_column(_("Name"), cols.name);
	recent_view->append_column(_("Score"), cols.score);
	recent_view->signal_cursor_changed().connect(sigc::mem_fun(*this, &Stats::on_cursor_changed));

	ranking_view->set_model(Gtk::ListStore::create(cols));
	ranking_view->append_column(_("Stroke"), cols.stroke);
	if (verbosity >= 4)
		ranking_view->append_column("Debug", cols.debug);
	ranking_view->append_column(_("Name"), cols.name);
	ranking_view->append_column(_("Score"), cols.score);
}

void Stats::on_cursor_changed() {
	Gtk::TreePath path;
	Gtk::TreeViewColumn *col;
	recent_view->get_cursor(path, col);
	Gtk::TreeRow row(*recent_store->get_iter(path));

	Glib::RefPtr<Gtk::ListStore> ranking_store = row[cols.child];
	ranking_view->set_model(ranking_store);
}

class Tooltip : public Gtk::Window {
public:
	Tooltip(Gtk::Widget &widget) : Gtk::Window(Gtk::WINDOW_POPUP) {
		Glib::RefPtr<Gdk::Visual> visual = get_screen()->get_rgba_visual();
		if (visual)
			gtk_widget_set_visual(GTK_WIDGET(gobj()), visual->gobj());
		set_type_hint(Gdk::WINDOW_TYPE_HINT_TOOLTIP);
		set_app_paintable();
		set_resizable(false);
		set_accept_focus(false);
		get_style_context()->add_class(GTK_STYLE_CLASS_TOOLTIP);
		signal_draw().connect(sigc::mem_fun(*this, &Tooltip::on_early_draw), false);
		add(widget);
		widget.show();
	}

private:
	bool on_early_draw(const Cairo::RefPtr<Cairo::Context> &cr) {
		if (is_composited()) {
			cr->save();
			cr->set_source_rgba(0, 0, 0, 0);
			cr->set_operator(Cairo::OPERATOR_SOURCE);
			cr->paint();
			cr->restore();
		}
		int w = get_allocated_width();
		int h = get_allocated_height();
		get_style_context()->render_background(cr, 0, 0, w, h);
		get_style_context()->render_frame(cr, 0, 0, w, h);
		return false;
	}
};

class Feedback {
	boost::shared_ptr<Gtk::Window> icon;
	boost::shared_ptr<Gtk::Window> text;
public:
	Feedback(RStroke s, Glib::ustring t, int x, int y) {
		x += (prefs.left_handed.get() ? 1 : -1)*3*STROKE_SIZE / 2;
		int w,h;
		if (s) {
			WIDGET(Gtk::Image, image, s->draw(STROKE_SIZE));
			image.set_padding(2,2);
			icon.reset(new Tooltip(image));
			icon->get_size(w,h);
			icon->move(x - w/2, y - h/2);
			y += h/2;
		}

		if (t != "") {
			WIDGET(Gtk::Label, label, t);
			label.set_padding(4,4);
			text.reset(new Tooltip(label));
			text->get_size(w,h);
			text->move(x - w/2, y + h/2);
		}
		if (text) {
			text->show();
			text->get_window()->input_shape_combine_region(Cairo::Region::create(), 0, 0);
		}
		if (icon) {
			icon->show();
			icon->get_window()->input_shape_combine_region(Cairo::Region::create(), 0, 0);
		}
	}
};

void Ranking::queue_show(RRanking r, RTriple e) {
	r->x = (int)e->x;
	r->y = (int)e->y;
	Glib::signal_idle().connect(sigc::bind(sigc::ptr_fun(&Ranking::show), r));
}

bool delete_me(boost::shared_ptr<Feedback>) {
	return false;
}

bool Ranking::show(RRanking r) {
	if (prefs.tray_feedback.get())
		win->set_icon(r->stroke, !r->best_stroke);
	if (prefs.feedback.get() && r->best_stroke) {
		if (prefs.advanced_popups.get() || !(r->best_stroke->button || r->best_stroke->timeout)) {
			boost::shared_ptr<Feedback> popup(new Feedback(r->best_stroke, r->name, r->x, r->y));
			Glib::signal_timeout().connect(sigc::bind(sigc::ptr_fun(&delete_me), popup), 600);
		}
	}
	Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*win->stats, &Stats::on_stroke), r), 200);
	return false;
}

Glib::ustring format_float(float x) {
	return Glib::ustring::format(std::fixed, std::setprecision(2), x);
}

Glib::RefPtr<Gdk::Pixbuf> Stroke::drawDebug(RStroke a, RStroke b, int size) {
	// TODO: This is copy'n'paste from win.cc
	Glib::RefPtr<Gdk::Pixbuf> pb = drawEmpty_(size);
	if (!a || !b || !a->stroke || !b->stroke)
		return pb;
	int w = size;
	int h = size;
	int stride = pb->get_rowstride();
	guint8 *row = pb->get_pixels();
	// This is all pretty messed up
	// http://www.archivum.info/gtkmm-list@gnome.org/2007-05/msg00112.html
	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create(row, Cairo::FORMAT_ARGB32, w, h, stride);
	const Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(surface);

	for (unsigned int s = 0; s+1 < a->size(); s++)
		for (unsigned int t = 0; t+1 < b->size(); t++) {
			double col = 1.0 - stroke_angle_difference(a->stroke.get(), b->stroke.get(), s, t);
			ctx->set_source_rgba(col,col,col,1.0);
			ctx->rectangle(a->time(s)*size, (1.0-b->time(t+1))*size,
					(a->time(s+1)-a->time(s))*size, (b->time(t+1)-b->time(t))*size);
			ctx->fill();
		}
	int path_x[a->size() + b->size()];
	int path_y[a->size() + b->size()];
	stroke_compare(a->stroke.get(), b->stroke.get(), path_x, path_y);
	ctx->set_source_rgba(1,0,0,1);
	ctx->set_line_width(2);
	ctx->move_to(size, 0);
	for (int i = 0;; i++) {
		ctx->line_to(a->time(path_x[i])*size, (1.0-b->time(path_y[i]))*size);
		if (!path_x[i] && !path_y[i])
			break;
	}
	ctx->stroke();

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

bool Stats::on_stroke(RRanking r) {
	Gtk::TreeModel::Row row = *(recent_store->prepend());
	row[cols.stroke] = r->stroke->draw(STROKE_SIZE);
	row[cols.name] = r->name;
	row[cols.score] = format_float(r->score*100) + "%";
	Glib::RefPtr<Gtk::ListStore> ranking_store = Gtk::ListStore::create(cols);
	row[cols.child] = ranking_store;

	Gtk::TreePath path = recent_store->get_path(row);
	recent_view->scroll_to_row(path);

	Gtk::TreeModel::Children ch = recent_store->children();
	if (ch.size() > 8) {
		Gtk::TreeIter last = ch.end();
		last--;
		recent_store->erase(last);

	}

	for (std::multimap<double, std::pair<std::string, RStroke> >::iterator i = r->r.begin(); i != r->r.end(); i++) {
		Gtk::TreeModel::Row row2 = *(ranking_store->prepend());
		row2[cols.stroke] = i->second.second->draw(STROKE_SIZE);
		if (verbosity >= 4)
			row2[cols.debug] = Stroke::drawDebug(r->stroke, i->second.second, STROKE_SIZE);
		row2[cols.name] = i->second.first;
		row2[cols.score] = format_float(i->first * 100) + "%";
	}
	return false;
}
