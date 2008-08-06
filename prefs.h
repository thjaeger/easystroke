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
#ifndef __PREFS_H__
#define __PREFS_H__
#include "queue.h"
#include "prefdb.h"

#include <gtkmm.h>

class Prefs {
public:
	Prefs();
	virtual ~Prefs() {}
private:
	void select_worker();
	void set_button_label();

	void on_selected(std::string &);
	void on_add();
	void on_remove();
	void on_p_changed();
	void on_p_default();
	void on_select_button();
	void on_trace_changed();

	struct SelectRow;

	Queue<std::string> q;

	class SingleColumn : public Gtk::TreeModel::ColumnRecord {
	public:
		SingleColumn() { add(col); }
		Gtk::TreeModelColumn<Glib::ustring> col;
	};
	SingleColumn cols;

	Glib::RefPtr<Gtk::ListStore> tm;
	Gtk::TreeView* tv;

	Gtk::ComboBox *trace;
	Gtk::ComboBoxText *click, *stroke_click;
	Gtk::HScale* scale_p;
	Gtk::SpinButton *spin_radius;
	Gtk::Label* blabel;
};

#endif
