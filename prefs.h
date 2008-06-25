#ifndef __PREFS_H__
#define __PREFS_H__
#include "queue.h"
#include "prefdb.h"

#include <gtkmm.h>

class Win;

class Prefs {
public:
	Prefs(Win *);
	virtual ~Prefs() {}
private:
	void select_worker();
	void set_button_label();

	void on_selected(std::string &);
	void on_add();
	void on_remove();
	void on_p_changed();
	void on_p_default();
	void on_delay_changed();
	void on_delay_default();
	void on_select_button();
	void on_trace_changed();
	void on_cds_stroke_changed();

	struct SelectRow;

	bool good_state;
	void write();

	Win* parent;

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
	Gtk::SpinButton* spin_delay;
	Gtk::Label* blabel;
	Gtk::CheckButton *cds_stroke;
};

#endif
