#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include <gtkmm.h>
#include "cellrenderertk/cellrenderertkmm.h"

class Win;
class ActionDBRef;

class Actions {
public:
	Actions();
private:
	void on_button_delete();
	void on_button_new();
	void on_button_record();
	void on_selection_changed();
	void on_cursor_changed();
	void on_name_edited(const Glib::ustring& path, const Glib::ustring& new_text);
	void on_cmd_edited(const Glib::ustring& path, const Glib::ustring& new_text);
	void on_type_edited(const Glib::ustring& path, const Glib::ustring& new_text);
	void on_accel_edited(const Glib::ustring& path_string, guint accel_key, Gdk::ModifierType accel_mods, guint hardware_keycode);
	void on_arg_editing_started(Gtk::CellEditable* editable, const Glib::ustring& path);
	void on_something_editing_started(Gtk::CellEditable* editable, const Glib::ustring& path);
	void on_something_editing_canceled();
	void on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);
	class OnStroke;
	Gtk::TreeRow get_selected_row();

	void update_arg(Glib::ustring);
	void focus(int id, int col, bool edit);

	bool good_state;
	void write(ActionDBRef&);

	class ModelColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		ModelColumns() { add(stroke); add(name); add(type); add(arg); add(cmd_save); add(id); }
		Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf> > stroke;
		Gtk::TreeModelColumn<Glib::ustring> name;
		Gtk::TreeModelColumn<Glib::ustring> type;
		Gtk::TreeModelColumn<Glib::ustring> arg;
		Gtk::TreeModelColumn<Glib::ustring> cmd_save;
		Gtk::TreeModelColumn<int> id;
	};
	ModelColumns cols;
	Gtk::TreeView* tv;
	Glib::RefPtr<Gtk::TreeStore> tm;

	class Single : public Gtk::TreeModel::ColumnRecord {
	public:
		Single() { add(type); }
		Gtk::TreeModelColumn<Glib::ustring> type;
	};
	Single type;

	struct Focus;

	Glib::RefPtr<Gtk::ListStore> type_store;

	Gtk::CellRendererCombo type_renderer;
	Gtk::CellRendererTK accel_renderer;

	Gtk::Button *button_record, *button_delete;

	bool editing_new;
	bool editing;
};

#endif
