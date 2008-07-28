#ifndef __STATS_H__
#define __STATS_H__
#include <gtkmm.h>
#include "queue.h"

class Ranking;

class Stats {
public:
	Stats();
	void stroke_push(Ranking *);
	virtual ~Stats();
private:
	void on_pdf();
	void on_stroke(Ranking *);
	void on_cursor_changed();

	class ModelColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		ModelColumns() { add(stroke); add(name); add(score); add(child); }

		Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf> > stroke;
		Gtk::TreeModelColumn<Glib::ustring> name;
		Gtk::TreeModelColumn<Glib::ustring> score;
		Gtk::TreeModelColumn<Glib::RefPtr<Gtk::ListStore> > child;
	};
	ModelColumns cols;

	Gtk::TreeView *recent_view;
	Glib::RefPtr<Gtk::ListStore> recent_store;

	Gtk::TreeView *ranking_view;

	class RankingQueue;
	RankingQueue *r_queue;
};

#endif
