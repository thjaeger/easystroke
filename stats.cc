#include "stats.h"
#include "win.h"
#include "actiondb.h"
#include <iomanip>

class Stats::RankingQueue : public Queue<Ranking *> {
public:
	RankingQueue(sigc::slot<void, Ranking *> cb) : Queue<Ranking *>(cb) {}
};

void Stats::stroke_push(Ranking *r) { r_queue->push(r); }

Stats::Stats() : r_queue(new RankingQueue(sigc::mem_fun(*this, &Stats::on_stroke))) {
	Gtk::Button *button_matrix;
	widgets->get_widget("button_matrix", button_matrix);
	widgets->get_widget("treeview_recent", recent_view);
	widgets->get_widget("treeview_ranking", ranking_view);

	button_matrix->signal_clicked().connect(sigc::mem_fun(*this, &Stats::on_pdf));

	recent_store = Gtk::ListStore::create(cols);
	recent_view->set_model(recent_store);
	recent_view->append_column("Stroke", cols.stroke);
	recent_view->append_column("Name", cols.name);
	recent_view->append_column("Score", cols.score);
	recent_view->signal_cursor_changed().connect(sigc::mem_fun(*this, &Stats::on_cursor_changed));

	ranking_view->set_model(Gtk::ListStore::create(cols));
	ranking_view->append_column("Stroke", cols.stroke);
	ranking_view->append_column("Name", cols.name);
	ranking_view->append_column("Score", cols.score);
}

void Stats::on_cursor_changed() {
	Gtk::TreePath path;
	Gtk::TreeViewColumn *col;
	recent_view->get_cursor(path, col);
	Gtk::TreeRow row(*recent_store->get_iter(path));

	Glib::RefPtr<Gtk::ListStore> ranking_store = row[cols.child];
	ranking_view->set_model(ranking_store);
}

Stats::~Stats() {
	delete r_queue;
}

void Stats::on_stroke(Ranking *r) {
	Gtk::TreeModel::Row row = *(recent_store->prepend());
	row[cols.stroke] = r->stroke->draw(STROKE_SIZE);
	row[cols.name] = r->name;
	row[cols.score] = Glib::ustring::format(std::fixed, std::setprecision(2), r->score*100) + "%";

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
		row2[cols.score] = Glib::ustring::format(std::fixed, std::setprecision(2), i->first * 100) + "%";
	}
	delete r;
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
	Atomic a;
	const ActionDB &as = actions.ref(a);
	const int n = as.nested_size();
	Cairo::RefPtr<Cairo::PdfSurface> surface = Cairo::PdfSurface::create("/tmp/strokes.pdf", (n+1)*S, (n+1)*S);
	const Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create(surface);
	int k = 1;
	for (StrokeIterator i = as.strokes_begin(); i; i++, k++) {
		i.stroke()->draw(surface, k*S+B, B, S-2*B, S-2*B, false);
		i.stroke()->draw(surface, B, k*S+B, S-2*B, S-2*B, false);

		ctx->set_source_rgba(0,0,0,1);
		ctx->set_line_width(1);
		ctx->move_to(k*S, B);
		ctx->line_to(k*S, (n+1)*S-B);
		ctx->move_to(B, k*S);
		ctx->line_to((n+1)*S-B, k*S);
		ctx->stroke();

		int l = 1;
		for (StrokeIterator j= as.strokes_begin(); j; j++, l++) {
			float score = Stroke::compare(i.stroke(), j.stroke());
			if (score > 0.7) {
				ctx->save();
				ctx->set_source_rgba(0,0,1,score-0.6);
				ctx->rectangle(l*S, k*S, S, S);
				ctx->fill();
				ctx->restore();
			}
			if (score < -1.5)
				continue;
			Glib::ustring str = Glib::ustring::format(std::fixed, std::setprecision(2), score);
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
	if (system("evince /tmp/strokes.pdf &") == -1)
		printf("Error: system() failed\n");
}

