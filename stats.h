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
#ifndef __STATS_H__
#define __STATS_H__
#include <gtkmm.h>

class Ranking;

class Stats {
public:
	Stats();
	bool on_stroke(Ranking *);
private:
	void on_pdf();
	void on_cursor_changed();

	class ModelColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		ModelColumns() { add(stroke); add(debug); add(name); add(score); add(child); }

		Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf> > stroke;
		Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf> > debug;
		Gtk::TreeModelColumn<Glib::ustring> name;
		Gtk::TreeModelColumn<Glib::ustring> score;
		Gtk::TreeModelColumn<Glib::RefPtr<Gtk::ListStore> > child;
	};
	ModelColumns cols;

	Gtk::TreeView *recent_view;
	Glib::RefPtr<Gtk::ListStore> recent_store;

	Gtk::TreeView *ranking_view;
};

#endif
