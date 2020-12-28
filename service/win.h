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
#ifndef __WIN_H__
#define __WIN_H__
#include "gesture.h"
#include <gtkmm.h>
#include "util.h"
#include "prefdb.h"

class Stats;
class Ranking;

// Convenience macro for on-the-fly creation of widgets
#define WIDGET(TYPE, NAME, ARGS...) TYPE &NAME = *Gtk::manage(new TYPE(ARGS))

extern Glib::RefPtr<Gtk::Builder> widgets;

class Win : Timeout {
public:
	Win();

	Gtk::Window& get_window() { return *win; }
	boost::shared_ptr<Stats> stats;
	void show();
	void hide();
	void show_hide();
	void set_icon(RStroke stroke, bool invert);
	void show_about();
private:
	bool on_icon_size_changed(int);
	virtual void timeout();
    void on_help_toggled();
	void show_popup(guint, guint32);
	void show_hide_icon();

	Gtk::Window *win;

	Gtk::Menu menu;

	Glib::RefPtr<Gtk::StatusIcon> icon;
	Glib::RefPtr<Gdk::Pixbuf> icon_pb[2];
};

extern Win *win;

class Stats {
public:
	Stats();
	bool on_stroke(boost::shared_ptr<Ranking>);
private:
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

class SelectButton {
public:
	SelectButton(ButtonInfo bi, bool def, bool any);
	~SelectButton();
	bool run();
	ButtonInfo event;
private:
	Gtk::MessageDialog *dialog;
	bool on_button_press(GdkEventButton *ev);
	void on_any_toggled();

	Gtk::EventBox *eventbox;
	Gtk::ToggleButton *toggle_shift, *toggle_control, *toggle_alt, *toggle_super, *toggle_any;
	Gtk::ComboBoxText *select_button;
	Gtk::RadioButton *radio_timeout_default, *radio_instant, *radio_click_hold;
	sigc::connection handler[2];
};

void error_dialog(const Glib::ustring &);
Glib::ustring app_name_hr(Glib::ustring);
#endif
