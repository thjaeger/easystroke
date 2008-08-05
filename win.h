#ifndef __WIN_H__
#define __WIN_H__
#include "stroke.h"
#include <gtkmm.h>
#include "queue.h"

class Actions;
class Prefs;
class Stats;
class Ranking;
class ButtonInfo;

// Convenience macro for on-the-fly creation of widgets
#define WIDGET(TYPE, NAME, ARGS...) TYPE &NAME = *Gtk::manage(new TYPE(ARGS))

int run_dialog(const char *);

extern Glib::RefPtr<Gtk::Builder> widgets;

class Win {
public:
	Win();
	virtual ~Win();
	void icon_push(RStroke s) { icon_queue.push(s); }
	void stroke_push(Ranking *);
	Gtk::Window& get_window() { return *win; }

	Glib::Dispatcher quit;

private:
	void on_icon_click();
	bool on_icon_size_changed(int);
	void on_icon_changed(RStroke s);
	void on_help_toggled();
	void show_popup(guint, guint32);

	Gtk::Window *win;
	Actions *actions;
	Prefs *prefs;
	Stats *stats;

	Gtk::Menu menu;

	Glib::RefPtr<Gtk::StatusIcon> icon;
	RStroke current_icon;
	Queue<RStroke> icon_queue;
};

class SelectButton {
public:
	SelectButton(ButtonInfo bi, bool def=true);
	~SelectButton();
	bool run();
	GdkEventButton event;
private:
	Gtk::Dialog *dialog;
	bool on_button_press(GdkEventButton *ev);

	Gtk::EventBox *eventbox;
	Gtk::ToggleButton *toggle_shift, *toggle_control, *toggle_alt, *toggle_super;
	Gtk::ComboBox *select_button;
	sigc::connection handler;
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
