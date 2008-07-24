#ifndef __PREFS_H__
#define __PREFS_H__
#include "queue.h"
#include "prefdb.h"

#include <gtkmm.h>

class Check {
protected:
	Lock<bool> &b;
	Gtk::CheckButton *check;
	virtual void on_changed();
public:
	Check(const Glib::ustring &, Lock<bool> &);
};

class Spin {
	friend class Pressure;
	Lock<int> &i;
	const int def;
	Gtk::SpinButton *spin;
	Gtk::Button *button;
	void on_changed();
	void on_default();
public:
	Spin(const Glib::ustring &, const Glib::ustring &, Lock<int> &, const int);
};

class Pressure : public Check {
	virtual void on_changed();
	Spin spin;
public:
	Pressure();
};

class Proximity : public Check {
	virtual void on_changed();
public:
	Proximity();
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
	Spin radius;
	Pressure pressure;
	Proximity proximity;
};

#endif
