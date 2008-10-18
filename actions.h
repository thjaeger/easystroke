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
#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include <gtkmm.h>

class Unique;
class Win;
class ActionListDiff;

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
	void on_combo_edited(const Glib::ustring& path_string, guint item);
	void on_arg_editing_started(Gtk::CellEditable* editable, const Glib::ustring& path);
	void on_something_editing_started(Gtk::CellEditable* editable, const Glib::ustring& path);
	void on_something_editing_canceled();
	void on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);
	void on_cell_data_name(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter);
	void on_cell_data_type(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter);
	void on_cell_data_arg(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter);
	class OnStroke;
	Gtk::TreeRow get_selected_row();

	void focus(Unique *id, int col, bool edit);

	struct SelectApp;
	void on_add_app();
	void on_app_selected(std::string);
	void on_apps_selection_changed();
	void load_app_list(const Gtk::TreeNodeChildren &ch, ActionListDiff *actions);
	void on_cell_data_apps(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter);

	class ModelColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		ModelColumns() {
			add(stroke); add(name); add(type); add(arg); add(cmd_save); add(id);
			add(name_bold); add(action_bold); add(deactivated);
		}
		Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf> > stroke;
		Gtk::TreeModelColumn<Glib::ustring> name, type, arg, cmd_save;
		Gtk::TreeModelColumn<Unique *> id;
		Gtk::TreeModelColumn<bool> name_bold, action_bold;
		Gtk::TreeModelColumn<bool> deactivated;
	};
	ModelColumns cols;
	Gtk::TreeView *tv;
	Glib::RefPtr<Gtk::ListStore> tm;

	Gtk::TreeView *apps_view;
	Glib::RefPtr<Gtk::TreeStore> apps_model;

	class Single : public Gtk::TreeModel::ColumnRecord {
	public:
		Single() { add(type); }
		Gtk::TreeModelColumn<Glib::ustring> type;
	};
	Single type;

	class Apps : public Gtk::TreeModel::ColumnRecord {
	public:
		Apps() { add(app); add(actions); }
		Gtk::TreeModelColumn<Glib::ustring> app;
		Gtk::TreeModelColumn<ActionListDiff *> actions;
	};
	Apps ca;

	struct Focus;

	Glib::RefPtr<Gtk::ListStore> type_store;

	Gtk::Button *button_record, *button_delete;

	bool editing_new;
	bool editing;
	
	ActionListDiff *action_list;
};

#endif
