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
#include "stroke.h"
#include <gtkmm.h>
#include "util.h"
#include "prefdb.h"

class Actions;
class Prefs;
class Stats;
class Ranking;

// Convenience macro for on-the-fly creation of widgets
#define WIDGET(TYPE, NAME, ARGS...) TYPE &NAME = *Gtk::manage(new TYPE(ARGS))

int run_dialog(const char *);

extern Glib::RefPtr<Gtk::Builder> widgets;

class CellRendererTextish : public Gtk::CellRendererText {
public:
	enum Mode { TEXT, KEY, POPUP, COMBO };
	Mode mode;
	const char **items;
	CellRendererTextish() : mode(TEXT) {}
	typedef sigc::signal<void, const Glib::ustring&, guint, Gdk::ModifierType, guint> key_edited;
	typedef sigc::signal<void, const Glib::ustring&, guint> combo_edited;
	key_edited &signal_key_edited() { return signal_key_edited_; }
	combo_edited &signal_combo_edited() { return signal_combo_edited_; }
protected:
	virtual Gtk::CellEditable* start_editing_vfunc(GdkEvent *event, Gtk::Widget &widget, const Glib::ustring &path,
			const Gdk::Rectangle &background_area, const Gdk::Rectangle &cell_area,
			Gtk::CellRendererState flags);
private:
	key_edited signal_key_edited_;
	combo_edited signal_combo_edited_;
};

class Win : Timeout {
public:
	Win();
	virtual ~Win();

	Gtk::Window& get_window() { return *win; }
	Actions *actions;
	Prefs *prefs_tab;
	Stats *stats;
	void show_hide();
	void show_success(bool good);
private:
	bool on_icon_size_changed(int);
	virtual void timeout();
	void on_help_toggled();
	void show_popup(guint, guint32);
	void show_hide_icon();
	void on_about();

	Gtk::Window *win;

	Gtk::Menu menu;

	Glib::RefPtr<Gtk::StatusIcon> icon;
	Glib::RefPtr<Gdk::Pixbuf> icon_pb[3];
};

extern Win *win;

class SelectButton {
public:
	SelectButton(ButtonInfo bi, bool def, bool any);
	~SelectButton();
	bool run();
	ButtonInfo event;
private:
	Gtk::Dialog *dialog;
	bool on_button_press(GdkEventButton *ev);
	void on_any_toggled();

	Gtk::EventBox *eventbox;
	Gtk::ToggleButton *toggle_shift, *toggle_control, *toggle_alt, *toggle_super, *toggle_any;
	Gtk::ComboBoxText *select_button;
	Gtk::CheckButton *check_instant;
	sigc::connection handler[2];
};

class FormatLabel {
public:
	FormatLabel(Glib::RefPtr<Gtk::Builder>, Glib::ustring, ...);
	~FormatLabel();
private:
	Glib::ustring oldstring;
	Gtk::Label *label;
};

class ErrorDialog : public Gtk::MessageDialog {
	virtual void on_response(int) { delete this; }
public:
	ErrorDialog(const Glib::ustring &);
};
#endif
