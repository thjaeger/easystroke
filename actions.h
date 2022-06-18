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
#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include <gtkmm.h>

class Unique;
class Win;
class ActionListDiff;

class TreeViewMulti : public Gtk::TreeView {
	bool pending;
	Gtk::TreePath path;
	virtual bool on_button_press_event(GdkEventButton* event);
	virtual bool on_button_release_event(GdkEventButton* event);
	virtual void on_drag_begin(const Glib::RefPtr<Gdk::DragContext> &context);
public:
	TreeViewMulti();
};

class Actions {
public:
	Actions();
private:
	void on_button_dup();
	void on_button_delete();
	void on_button_new();
	void on_button_record();
	void on_selection_changed();
	void on_name_edited(const Glib::ustring& path, const Glib::ustring& new_text);
	void on_type_edited(const Glib::ustring& path, const Glib::ustring& new_text);
	void on_something_editing_started(Gtk::CellEditable* editable, const Glib::ustring& path);
	void on_something_editing_canceled();
	void on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);
	void on_cell_data_name(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter);
	void on_cell_data_type(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter);
public:
	void on_accel_edited(const gchar *path_string, guint accel_key, GdkModifierType accel_mods, guint hardware_keycode);
	void on_combo_edited(const gchar *path_string, guint item);
	void on_arg_editing_started(GtkCellEditable *editable, const gchar *path);
	void on_text_edited(const gchar *path, const gchar *new_text);
	void on_cell_data_arg(GtkCellRenderer *cell, gchar *path);
private:
	int compare_ids(const Gtk::TreeModel::iterator &a, const Gtk::TreeModel::iterator &b);
	class OnStroke;
	Gtk::TreeRow get_selected_row();

	void focus(Unique *id, int col, bool edit);
	bool do_focus(Unique *id, Gtk::TreeViewColumn *col, bool edit);

	bool select_app(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter, ActionListDiff *actions);
	void on_add_app();
	void on_add_group();
	void on_group_name_edited(const Glib::ustring& path, const Glib::ustring& new_text);
	void on_apps_selection_changed();
	void on_expanded();
	void load_app_list(const Gtk::TreeNodeChildren &ch, ActionListDiff *actions);
	void on_cell_data_apps(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter);
	void update_action_list();
	void update_row(const Gtk::TreeRow &row);
	bool count_app_actions(const Gtk::TreeIter &i);
	void update_counts();
	void on_reset_actions();
	void on_remove_app();

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
	class Store : public Gtk::ListStore {
		Actions *parent;
	public:
		Store(const Gtk::TreeModelColumnRecord &columns, Actions *p) : Gtk::ListStore(columns), parent(p) {}
		static Glib::RefPtr<Store> create(const Gtk::TreeModelColumnRecord &columns, Actions *parent) {
			return Glib::RefPtr<Store>(new Store(columns, parent));
		}
	protected:
		bool row_draggable_vfunc(const Gtk::TreeModel::Path &path) const;
		bool row_drop_possible_vfunc(const Gtk::TreeModel::Path &dest, const Gtk::SelectionData &selection) const;
		bool drag_data_received_vfunc(const Gtk::TreeModel::Path &dest, const Gtk::SelectionData& selection);
	};
	class AppsStore : public Gtk::TreeStore {
		Actions *parent;
	public:
		AppsStore(const Gtk::TreeModelColumnRecord &columns, Actions *p) : Gtk::TreeStore(columns), parent(p) {}
		static Glib::RefPtr<AppsStore> create(const Gtk::TreeModelColumnRecord &columns, Actions *parent) {
			return Glib::RefPtr<AppsStore>(new AppsStore(columns, parent));
		}
	protected:
		bool row_drop_possible_vfunc(const Gtk::TreeModel::Path &dest, const Gtk::SelectionData &selection) const;
		bool drag_data_received_vfunc(const Gtk::TreeModel::Path &dest, const Gtk::SelectionData& selection);
	};
	ModelColumns cols;
	TreeViewMulti tv;
	Glib::RefPtr<Store> tm;

	Gtk::TreeView *apps_view;
	Glib::RefPtr<AppsStore> apps_model;

	class Single : public Gtk::TreeModel::ColumnRecord {
	public:
		Single() { add(type); }
		Gtk::TreeModelColumn<Glib::ustring> type;
	};
	Single type;

	class Apps : public Gtk::TreeModel::ColumnRecord {
	public:
		Apps() { add(app); add(actions); add(count); }
		Gtk::TreeModelColumn<Glib::ustring> app;
		Gtk::TreeModelColumn<ActionListDiff *> actions;
		Gtk::TreeModelColumn<int> count;
	};
	Apps ca;

	struct Focus;

	Glib::RefPtr<Gtk::ListStore> type_store;

	Gtk::Button *button_record, *button_delete, *button_remove_app, *button_reset_actions, *button_dup;
	Gtk::CheckButton *check_show_deleted;
	Gtk::Expander *expander_apps;
	Gtk::VPaned *vpaned_apps;

	int vpaned_position;
	bool editing_new;
	bool editing;

	ActionListDiff *action_list;
};

#endif
