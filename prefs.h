#ifndef __PREFS_H__
#define __PREFS_H__
#include "queue.h"
#include "prefdb.h"

#include <gtkmm.h>

class Check {
	Lock<bool> &b;
	Gtk::CheckButton *check;
public:
	Check(const Glib::ustring &, Lock<bool> &);
	void on_changed();
};

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
	void on_radius_changed();
	void on_radius_default();
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
	Check advanced_ignore, ignore_grab, timing_workaround, show_clicks;
};

#endif
