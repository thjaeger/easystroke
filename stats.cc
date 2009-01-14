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
#include "stats.h"
#include "win.h"
#include "actiondb.h"
#include <iomanip>
#include <glibmm/i18n.h>

Stats::Stats() {
	Gtk::Button *button_matrix;
	widgets->get_widget("button_matrix", button_matrix);
	widgets->get_widget("treeview_recent", recent_view);
	widgets->get_widget("treeview_ranking", ranking_view);

	button_matrix->signal_clicked().connect(sigc::mem_fun(*this, &Stats::on_pdf));

	recent_store = Gtk::ListStore::create(cols);
	recent_view->set_model(recent_store);
	recent_view->append_column(_("Stroke"), cols.stroke);
	recent_view->append_column(_("Name"), cols.name);
	recent_view->append_column(_("Score"), cols.score);
	recent_view->signal_cursor_changed().connect(sigc::mem_fun(*this, &Stats::on_cursor_changed));

	ranking_view->set_model(Gtk::ListStore::create(cols));
	ranking_view->append_column(_("Stroke"), cols.stroke);
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

class Feedback {
	Gtk::Window *icon;
	Gtk::Window *text;
public:
	Feedback(RStroke s, Glib::ustring t, int x, int y) : icon(0), text(0) {
		x += (prefs.left_handed.get() ? 1 : -1)*3*STROKE_SIZE / 2;
		int w,h;
		if (s) {
			icon = new Gtk::Window(Gtk::WINDOW_POPUP);
			WIDGET(Gtk::Image, image, s->draw(STROKE_SIZE));
			icon->set_accept_focus(false);
			icon->modify_bg(Gtk::STATE_NORMAL, Gdk::Color("LemonChiffon"));
			icon->add(image);
			image.show();
			icon->get_size(w,h);
			icon->move(x - w/2, y - h/2);
			y += h/2;
		}

		if (t != "") {
			text = new Gtk::Window(Gtk::WINDOW_POPUP);
			text->set_accept_focus(false);
			text->set_border_width(2);
			WIDGET(Gtk::Label, label, t);
			text->modify_bg(Gtk::STATE_NORMAL, Gdk::Color("LemonChiffon"));
			text->add(label);
			label.show();
			text->get_size(w,h);
			text->move(x - w/2, y + h/2);
		}
		if (text) {
			text->show();
			text->get_window()->input_shape_combine_region(Gdk::Region(), 0, 0);
		}
		if (icon) {
			icon->show();
			icon->get_window()->input_shape_combine_region(Gdk::Region(), 0, 0);
		}
	}
	bool destroy() {
		if (icon) {
			icon->hide();
			delete icon;
		}
		if (text) {
			text->hide();
			delete text;
		}
		delete this;
		return false;
	}
};

bool Ranking::show() {
	win->stats->on_stroke(this);
	delete this;
	return false;
}

Glib::ustring format_float(float x) {
	return Glib::ustring::format(std::fixed, std::setprecision(2), x);
}

void Stats::on_stroke(Ranking *r) {
	if (prefs.feedback.get() && r->best_stroke) {
		if (prefs.advanced_popups.get() || !(r->best_stroke->button || r->best_stroke->timeout)) {
			Feedback *popup = new Feedback(r->best_stroke, r->name, r->x, r->y);
			Glib::signal_timeout().connect(sigc::mem_fun(*popup, &Feedback::destroy), 600);
		}
	}

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
		row2[cols.name] = i->second.first;
		row2[cols.score] = format_float(i->first * 100) + "%";
	}
}

#define GRAPH 0
#define TRIANGLE 0

#if TRIANGLE
double abs(double x) {
	return x > 0 ? x : (-x);
}

void test_triangle_inequality() {
	int bad = 0;
	int good = 0;
	double worst = 0;
	std::string worsti, worstj, worstk;
	Ref<ActionDB> ref(actions());
	for (StrokeIterator i = ref->strokes_begin(); i; i++) {
		for (StrokeIterator j = ref->strokes_begin(); j; j++) {
			if (i.stroke() == j.stroke())
				continue;
			for (StrokeIterator k = ref->strokes_begin(); k; k++) {
				if (i.stroke() == k.stroke())
					continue;
				if (j.stroke() == k.stroke())
					continue;
				double scoreij = Stroke::compare(i.stroke(), j.stroke());
				double scorejk = Stroke::compare(j.stroke(), k.stroke());
				double scoreik = Stroke::compare(i.stroke(), k.stroke());
//				double score = abs(scoreij*scorejk/scoreik);
				double score = 1-abs(scoreik) - (1-abs(scoreij) + 1-abs(scorejk));
				if (score > worst) {
					worst = score;
					worsti = i.name();
					worstj = j.name();
					worstk = k.name();
				}
				if (1-abs(scoreij) + 1-abs(scorejk) < 1-abs(scoreik))
//				if (abs(scoreij * scorejk) > abs(scoreik))
					bad++;
				else
					good++;
			}
		}
	}
	printf("good: %d, bad: %d\n", good, bad);
	std::cout << "worst: " << worsti << ", " << worstj << ", " << worstk << " (" << worst << ")" << std::endl;
}
#endif


void Stats::on_pdf() {
#if TRIANGLE
	test_triangle_inequality();
#endif
	{
	const int S = 32;
	const int B = 1;
	std::list<RStroke> strokes;
	actions.get_root()->all_strokes(strokes);
	const int n = strokes.size();
	Cairo::RefPtr<Cairo::PdfSurface> surface = Cairo::PdfSurface::create("/tmp/strokes.pdf", (n+1)*S, (n+1)*S);
	const Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(surface);
	int k = 1;
	for (std::list<RStroke>::iterator i = strokes.begin(); i != strokes.end(); i++, k++) {
		(*i)->draw(surface, k*S+B, B, S-2*B, S-2*B, false);
		(*i)->draw(surface, B, k*S+B, S-2*B, S-2*B, false);

		ctx->set_source_rgba(0,0,0,1);
		ctx->set_line_width(1);
		ctx->move_to(k*S, B);
		ctx->line_to(k*S, (n+1)*S-B);
		ctx->move_to(B, k*S);
		ctx->line_to((n+1)*S-B, k*S);
		ctx->stroke();

		int l = 1;
		for (std::list<RStroke>::iterator j = strokes.begin(); j != strokes.end(); j++, l++) {
			double score;
		        bool match = Stroke::compare(*i, *j, score);
			if (match) {
				ctx->save();
				ctx->set_source_rgba(0,0,1,score-0.6);
				ctx->rectangle(l*S, k*S, S, S);
				ctx->fill();
				ctx->restore();
			}
			if (score < -1.5)
				continue;
			Glib::ustring str = format_float(score);
			Cairo::TextExtents te;
			ctx->get_text_extents(str, te);
			ctx->move_to(l*S+S/2 - te.x_bearing - te.width/2, k*S+S/2 - te.y_bearing - te.height/2);
			ctx->show_text(str);
#if GRAPH
			score = Stroke::compare(i.stroke(), j.stroke(), 0);
			ctx->move_to(l*S, k*S + (1-score)*S/2);
			for (int p = 1; p!= 16; p++) {
				score = Stroke::compare(i.stroke(), j.stroke(), p/15.0);
				ctx->line_to(l*S+p*S/15.0, k*S + (1-score)*S/2);
			}
			ctx->stroke();
#endif
		}
	}
	}
	if (!fork()) {
		execlp("evince", "evince", "/tmp/strokes.pdf", NULL);
		exit(EXIT_FAILURE);
	}
}

