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
#include "actions.h"
#include "actiondb.h"
#include "win.h"
#include "main.h"
#include "prefdb.h"
#include <glibmm/i18n.h>
#include <X11/XKBlib.h>
#include "grabber.h"
#include "cellrenderertextish.h"

#include <functional>
#include <typeinfo>

bool TreeViewMulti::on_button_press_event(GdkEventButton* event) {
	int cell_x, cell_y;
	Gtk::TreeViewColumn *column;
	pending = (get_path_at_pos(event->x, event->y, path, column, cell_x, cell_y))
		&& (get_selection()->is_selected(path))
		&& !(event->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK));
	return Gtk::TreeView::on_button_press_event(event);
}

bool TreeViewMulti::on_button_release_event(GdkEventButton* event) {
	if (pending) {
		pending = false;
		get_selection()->unselect_all();
		get_selection()->select(path);
	}
	return Gtk::TreeView::on_button_release_event(event);
}

void TreeViewMulti::on_drag_begin(const Glib::RefPtr<Gdk::DragContext> &context) {
	pending = false;
	if (get_selection()->count_selected_rows() <= 1)
		return Gtk::TreeView::on_drag_begin(context);
	Glib::RefPtr<Gdk::Pixbuf> pb = render_icon_pixbuf(Gtk::Stock::DND_MULTIPLE, Gtk::ICON_SIZE_DND);
	context->set_icon(pb, pb->get_width(), pb->get_height());
}

bool negate(bool b) { return !b; }

TreeViewMulti::TreeViewMulti() : Gtk::TreeView(), pending(false) {
  get_selection()->set_select_function([this](const Glib::RefPtr<Gtk::TreeModel>&, const Gtk::TreePath&, const bool&) { return !this->pending; });
}

enum Type { COMMAND, KEY, TEXT, SCROLL, IGNORE, BUTTON, MISC };

struct TypeInfo {
	Type type;
	const char *name;
	const std::type_info *type_info;
	const CellRendererTextishMode mode;
};

TypeInfo all_types[8] = {
	{ COMMAND, N_("Command"), &typeid(Command),  CELL_RENDERER_TEXTISH_MODE_Text  },
	{ KEY,     N_("Key"),     &typeid(SendKey),  CELL_RENDERER_TEXTISH_MODE_Key   },
	{ TEXT,    N_("Text"),    &typeid(SendText), CELL_RENDERER_TEXTISH_MODE_Text  },
	{ SCROLL,  N_("Scroll"),  &typeid(Scroll),   CELL_RENDERER_TEXTISH_MODE_Key   },
	{ IGNORE,  N_("Ignore"),  &typeid(Ignore),   CELL_RENDERER_TEXTISH_MODE_Key   },
	{ BUTTON,  N_("Button"),  &typeid(Button),   CELL_RENDERER_TEXTISH_MODE_Popup },
	{ MISC,    N_("Misc"),    &typeid(Misc),     CELL_RENDERER_TEXTISH_MODE_Combo },
	{ COMMAND, 0,             0,                 CELL_RENDERER_TEXTISH_MODE_Text  }
};

const Type from_name(Glib::ustring name) {
	for (TypeInfo *i = all_types;; i++)
		if (!i->name || _(i->name) == name)
			return i->type;
}

const char *type_info_to_name(const std::type_info *info) {
	for (TypeInfo *i = all_types; i->name; i++)
		if (i->type_info == info)
			return _(i->name);
	return "";
}

static void on_actions_cell_data_arg(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data) {
	GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
	gchar *path_string = gtk_tree_path_to_string(path);
	((Actions *)data)->on_cell_data_arg(cell, path_string);
	g_free(path_string);
	gtk_tree_path_free(path);
}

static void on_actions_accel_edited(CellRendererTextish *, gchar *path, GdkModifierType mods, guint code, gpointer data) {
	guint key = XkbKeycodeToKeysym(dpy, code, 0, 0);
	((Actions *)data)->on_accel_edited(path, key, mods, code);
}

static void on_actions_combo_edited(CellRendererTextish *, gchar *path, guint row, gpointer data) {
	((Actions *)data)->on_combo_edited(path, row);
}

static void on_actions_text_edited(GtkCellRendererText *, gchar *path, gchar *new_text, gpointer data) {
	((Actions *)data)->on_text_edited(path, new_text);
}

static void on_actions_editing_started(GtkCellRenderer *, GtkCellEditable *editable, const gchar *path, gpointer data) {
	((Actions *)data)->on_arg_editing_started(editable, path);
}

Actions::Actions() :
	apps_view(0),
	vpaned_position(-1),
	editing_new(false),
	editing(false),
	action_list(actions.get_root())
{
	Gtk::ScrolledWindow *sw;
	widgets->get_widget("scrolledwindow_actions", sw);
	widgets->get_widget("treeview_apps", apps_view);
	sw->add(tv);
	tv.show();

	Gtk::Button *button_add, *button_add_app, *button_add_group;
	widgets->get_widget("button_add_action", button_add);
	widgets->get_widget("button_delete_action", button_delete);
	widgets->get_widget("button_record", button_record);
	widgets->get_widget("button_add_app", button_add_app);
	widgets->get_widget("button_add_group", button_add_group);
	widgets->get_widget("button_remove_app", button_remove_app);
	widgets->get_widget("button_reset_actions", button_reset_actions);
	widgets->get_widget("check_show_deleted", check_show_deleted);
	widgets->get_widget("expander_apps", expander_apps);
	widgets->get_widget("vpaned_apps", vpaned_apps);
	button_record->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_record));
	button_delete->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_delete));
	button_add->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_new));
	button_add_app->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_add_app));
	button_add_group->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_add_group));
	button_remove_app->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_remove_app));
	button_reset_actions->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_reset_actions));

	tv.signal_row_activated().connect(sigc::mem_fun(*this, &Actions::on_row_activated));
	tv.get_selection()->signal_changed().connect(sigc::mem_fun(*this, &Actions::on_selection_changed));

	tm = Store::create(cols, this);

	tv.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);

	int n;
	n = tv.append_column(_("Stroke"), cols.stroke);
	tv.get_column(n-1)->set_sort_column(cols.id);
	tm->set_sort_func(cols.id, sigc::mem_fun(*this, &Actions::compare_ids));
	tm->set_default_sort_func(sigc::mem_fun(*this, &Actions::compare_ids));
	tm->set_sort_column(Gtk::TreeSortable::DEFAULT_SORT_COLUMN_ID, Gtk::SORT_ASCENDING);

	n = tv.append_column(_("Name"), cols.name);
	Gtk::CellRendererText *name_renderer = dynamic_cast<Gtk::CellRendererText *>(tv.get_column_cell_renderer(n-1));
	name_renderer->property_editable() = true;
	name_renderer->signal_edited().connect(sigc::mem_fun(*this, &Actions::on_name_edited));
	name_renderer->signal_editing_started().connect(sigc::mem_fun(*this, &Actions::on_something_editing_started));
	name_renderer->signal_editing_canceled().connect(sigc::mem_fun(*this, &Actions::on_something_editing_canceled));
	Gtk::TreeView::Column *col_name = tv.get_column(n-1);
	col_name->set_sort_column(cols.name);
	col_name->set_cell_data_func(*name_renderer, sigc::mem_fun(*this, &Actions::on_cell_data_name));
	col_name->set_resizable();

	type_store = Gtk::ListStore::create(type);
	for (TypeInfo *i = all_types; i->name; i++)
		(*(type_store->append()))[type.type] = _(i->name);

	Gtk::CellRendererCombo *type_renderer = Gtk::manage(new Gtk::CellRendererCombo);
	type_renderer->property_model() = type_store;
	type_renderer->property_editable() = true;
	type_renderer->property_text_column() = 0;
	type_renderer->property_has_entry() = false;
	type_renderer->signal_edited().connect(sigc::mem_fun(*this, &Actions::on_type_edited));
	type_renderer->signal_editing_started().connect(sigc::mem_fun(*this, &Actions::on_something_editing_started));
	type_renderer->signal_editing_canceled().connect(sigc::mem_fun(*this, &Actions::on_something_editing_canceled));

	n = tv.append_column(_("Type"), *type_renderer);
	Gtk::TreeView::Column *col_type = tv.get_column(n-1);
	col_type->add_attribute(type_renderer->property_text(), cols.type);
	col_type->set_cell_data_func(*type_renderer, sigc::mem_fun(*this, &Actions::on_cell_data_type));

	int i = 0;
	while (Misc::types[i]) i++;
	CellRendererTextish *arg_renderer = cell_renderer_textish_new_with_items ((gchar**)Misc::types, i);
	GtkTreeViewColumn *col_arg = gtk_tree_view_column_new_with_attributes(_("Details"), GTK_CELL_RENDERER (arg_renderer), "text", cols.arg.index(), nullptr);
	gtk_tree_view_append_column(tv.gobj(), col_arg);

	gtk_tree_view_column_set_cell_data_func (col_arg, GTK_CELL_RENDERER (arg_renderer), on_actions_cell_data_arg, this, nullptr);
	gtk_tree_view_column_set_resizable(col_arg, true);
	g_object_set(arg_renderer, "editable", true, nullptr);
	g_signal_connect(arg_renderer, "key-edited", G_CALLBACK(on_actions_accel_edited), this);
	g_signal_connect(arg_renderer, "combo-edited", G_CALLBACK(on_actions_combo_edited), this);
	g_signal_connect(arg_renderer, "edited", G_CALLBACK(on_actions_text_edited), this);
	g_signal_connect(arg_renderer, "editing-started", G_CALLBACK(on_actions_editing_started), this);

	update_action_list();
	tv.set_model(tm);
	tv.enable_model_drag_source();
	tv.enable_model_drag_dest();

	check_show_deleted->signal_toggled().connect(sigc::mem_fun(*this, &Actions::update_action_list));
	expander_apps->property_expanded().signal_changed().connect(sigc::mem_fun(*this, &Actions::on_apps_selection_changed));
	expander_apps->property_expanded().signal_changed().connect(sigc::mem_fun(*this, &Actions::on_expanded));
	apps_view->get_selection()->signal_changed().connect(sigc::mem_fun(*this, &Actions::on_apps_selection_changed));
	apps_model = AppsStore::create(ca, this);

	load_app_list(apps_model->children(), actions.get_root());
	update_counts();

	apps_view->append_column_editable(_("Application"), ca.app);
	apps_view->get_column(0)->set_expand(true);
	apps_view->get_column(0)->set_cell_data_func(
			*apps_view->get_column_cell_renderer(0), sigc::mem_fun(*this, &Actions::on_cell_data_apps));
	Gtk::CellRendererText *app_name_renderer =
		dynamic_cast<Gtk::CellRendererText *>(apps_view->get_column_cell_renderer(0));
	app_name_renderer->signal_edited().connect(sigc::mem_fun(*this, &Actions::on_group_name_edited));
	apps_view->append_column(_("Actions"), ca.count);

	apps_view->set_model(apps_model);
	apps_view->enable_model_drag_dest();
	apps_view->expand_all();
}

void Actions::load_app_list(const Gtk::TreeNodeChildren &ch, ActionListDiff *actions) {
	Gtk::TreeRow row = *(apps_model->append(ch));
	row[ca.app] = app_name_hr(actions->name);
	row[ca.actions] = actions;
	for (ActionListDiff::iterator i = actions->begin(); i != actions->end(); i++)
		load_app_list(row.children(), &(*i));
}

void Actions::on_cell_data_name(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter) {
	bool bold = (*iter)[cols.name_bold];
	bool deactivated = (*iter)[cols.deactivated];
	Gtk::CellRendererText *renderer = dynamic_cast<Gtk::CellRendererText *>(cell);
	if (renderer)
		renderer->property_weight().set_value(bold ? 700 : 400);
	cell->property_sensitive().set_value(!deactivated);
}

void Actions::on_cell_data_type(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter) {
	bool bold = (*iter)[cols.action_bold];
	bool deactivated = (*iter)[cols.deactivated];
	Gtk::CellRendererText *renderer = dynamic_cast<Gtk::CellRendererText *>(cell);
	if (renderer)
		renderer->property_weight().set_value(bold ? 700 : 400);
	cell->property_sensitive().set_value(!deactivated);
}

void Actions::on_cell_data_arg(GtkCellRenderer *cell, gchar *path) {
	Gtk::TreeModel::iterator iter = tm->get_iter(path);
	bool bold = (*iter)[cols.action_bold];
	bool deactivated = (*iter)[cols.deactivated];
	g_object_set(cell, "sensitive", !deactivated, "weight", bold ? 700 : 400, nullptr);
	CellRendererTextish *renderer = CELL_RENDERER_TEXTISH (cell);
	if (!renderer)
		return;
	Glib::ustring str = (*iter)[cols.type];
	renderer->mode = all_types[from_name(str)].mode;
}

int Actions::compare_ids(const Gtk::TreeModel::iterator &a, const Gtk::TreeModel::iterator &b) {
	Unique *x = (*a)[cols.id];
	Unique *y = (*b)[cols.id];
	if (x->level == y->level) {
		if (x->i == y->i)
			return 0;
		if (x->i < y->i)
			return -1;
		else
			return 1;
	}
	if (x->level < y->level)
		return -1;
	else
		return 1;
}

bool Actions::AppsStore::row_drop_possible_vfunc(const Gtk::TreeModel::Path &dest,
		const Gtk::SelectionData &selection) const {
	static bool expecting = false;
	static Gtk::TreePath expected;
	if (expecting && expected != dest)
		expecting = false;
	if (!expecting) {
		if (gtk_tree_path_get_depth((GtkTreePath *)dest.gobj()) < 2 || dest.back() != 0)
			return false;
		expected = dest;
		expected.up();
		expecting = true;
		return false;
	}
	expecting = false;
	Gtk::TreePath src;
	Glib::RefPtr<TreeModel> model;
	if (!Gtk::TreeModel::Path::get_from_selection_data(selection, model, src))
		return false;
	if (model != parent->tm)
		return false;
	Gtk::TreeIter dest_iter = parent->apps_model->get_iter(dest);
	ActionListDiff *actions = dest_iter ? (*dest_iter)[parent->ca.actions] : (ActionListDiff *)nullptr;
	return actions && actions != parent->action_list;
}

bool Actions::AppsStore::drag_data_received_vfunc(const Gtk::TreeModel::Path &dest, const Gtk::SelectionData &selection) {
	Gtk::TreePath src;
	Glib::RefPtr<TreeModel> model;
	if (!Gtk::TreeModel::Path::get_from_selection_data(selection, model, src))
		return false;
	if (model != parent->tm)
		return false;
	Unique *src_id = (*parent->tm->get_iter(src))[parent->cols.id];
	Gtk::TreeIter dest_iter = parent->apps_model->get_iter(dest);
	ActionListDiff *actions = dest_iter ? (*dest_iter)[parent->ca.actions] : (ActionListDiff *)nullptr;
	if (!actions || actions == parent->action_list)
		return false;
	Glib::RefPtr<Gtk::TreeSelection> sel = parent->tv.get_selection();
	if (sel->count_selected_rows() <= 1) {
		RStrokeInfo si = parent->action_list->get_info(src_id);
		parent->action_list->remove(src_id);
		actions->add(*si);
	} else {
		std::vector<Gtk::TreePath> paths = sel->get_selected_rows();
		for (std::vector<Gtk::TreePath>::iterator i = paths.begin(); i != paths.end(); ++i) {
			Unique *id = (*parent->tm->get_iter(*i))[parent->cols.id];
			RStrokeInfo si = parent->action_list->get_info(id);
			parent->action_list->remove(id);
			actions->add(*si);
		}
	}
	parent->update_action_list();
	update_actions();
	return true;
}

bool Actions::Store::row_draggable_vfunc(const Gtk::TreeModel::Path &path) const {
	int col;
	Gtk::SortType sort;
	parent->tm->get_sort_column_id(col, sort);
	if (col != Gtk::TreeSortable::DEFAULT_SORT_COLUMN_ID)
		return false;
	if (sort != Gtk::SORT_ASCENDING)
		return false;
	Glib::RefPtr<Gtk::TreeSelection> sel = parent->tv.get_selection();
	if (sel->count_selected_rows() <= 1) {
		Unique *id = (*parent->tm->get_iter(path))[parent->cols.id];
		return id->level == parent->action_list->level;
	} else {
		std::vector<Gtk::TreePath> paths = sel->get_selected_rows();
		for (std::vector<Gtk::TreePath>::iterator i = paths.begin(); i != paths.end(); ++i) {
			Unique *id = (*parent->tm->get_iter(*i))[parent->cols.id];
			if (id->level != parent->action_list->level)
				return false;
		}
		return true;
	}
}

bool Actions::Store::row_drop_possible_vfunc(const Gtk::TreeModel::Path &dest, const Gtk::SelectionData &selection) const {
	static bool ignore_next = false;
	if (gtk_tree_path_get_depth((GtkTreePath *)dest.gobj()) > 1) {
		ignore_next = true;
		return false;
	}
	if (ignore_next) {
		ignore_next = false;
		return false;
	}
	Gtk::TreePath src;
	Glib::RefPtr<TreeModel> model;
	if (!Gtk::TreeModel::Path::get_from_selection_data(selection, model, src))
		return false;
	if (model != parent->tm)
		return false;
	Unique *src_id = (*parent->tm->get_iter(src))[parent->cols.id];
	Gtk::TreeIter dest_iter = parent->tm->get_iter(dest);
	Unique *dest_id = dest_iter ? (*dest_iter)[parent->cols.id] : (Unique *)0;
	if (dest_id && src_id->level != dest_id->level)
		return false;
	return true;
}

bool Actions::Store::drag_data_received_vfunc(const Gtk::TreeModel::Path &dest, const Gtk::SelectionData &selection) {
	Gtk::TreePath src;
	Glib::RefPtr<TreeModel> model;
	if (!Gtk::TreeModel::Path::get_from_selection_data(selection, model, src))
		return false;
	if (model != parent->tm)
		return false;
	Unique *src_id = (*parent->tm->get_iter(src))[parent->cols.id];
	Gtk::TreeIter dest_iter = parent->tm->get_iter(dest);
	Unique *dest_id = dest_iter ? (*dest_iter)[parent->cols.id] : (Unique *)0;
	if (dest_id && src_id->level != dest_id->level)
		return false;
	Glib::RefPtr<Gtk::TreeSelection> sel = parent->tv.get_selection();
	if (sel->count_selected_rows() <= 1) {
		if (parent->action_list->move(src_id, dest_id)) {
			(*parent->tm->get_iter(src))[parent->cols.id] = src_id;
			update_actions();
		}
	} else {
		std::vector<Gtk::TreePath> paths = sel->get_selected_rows();
		bool updated = false;
		for (std::vector<Gtk::TreePath>::iterator i = paths.begin(); i != paths.end(); ++i) {
			Unique *id = (*parent->tm->get_iter(*i))[parent->cols.id];
			if (parent->action_list->move(id, dest_id))
				updated = true;
		}
		if (updated) {
			parent->update_action_list();
			update_actions();
		}
	}
	return false;
}

void Actions::on_type_edited(const Glib::ustring &path, const Glib::ustring &new_text) {
	tv.grab_focus();
	Gtk::TreeRow row(*tm->get_iter(path));
	Type new_type = from_name(new_text);
	Type old_type = from_name(row[cols.type]);
	bool edit = true;
	if (old_type == new_type) {
		edit = editing_new;
	} else {
		row[cols.type] = new_text;
		RAction new_action;
		if (new_type == COMMAND) {
			Glib::ustring cmd_save = row[cols.cmd_save];
			if (cmd_save != "")
				edit = false;
			new_action = Command::create(cmd_save);
		}
		if (old_type == COMMAND) {
			row[cols.cmd_save] = (Glib::ustring)row[cols.arg];
		}
		if (new_type == KEY) {
			new_action = SendKey::create(0, (Gdk::ModifierType)0);
			edit = true;
		}
		if (new_type == TEXT) {
			new_action = SendText::create(Glib::ustring());
			edit = true;
		}
		if (new_type == SCROLL) {
			new_action = Scroll::create((Gdk::ModifierType)0);
			edit = false;
		}
		if (new_type == IGNORE) {
			new_action = Ignore::create((Gdk::ModifierType)0);
			edit = false;
		}
		if (new_type == BUTTON) {
			new_action = Button::create((Gdk::ModifierType)0, 0);
			edit = true;
		}
		if (new_type == MISC) {
			new_action = Misc::create(Misc::NONE);
			edit = true;
		}
		action_list->set_action(row[cols.id], new_action);
		update_row(row);
		update_actions();
	}
	editing_new = false;
	focus(row[cols.id], 3, edit);
}

void Actions::on_button_delete() {
	int n = tv.get_selection()->count_selected_rows();

	Glib::ustring str;
	if (n == 1)
		str = Glib::ustring::compose(_("Action \"%1\" is about to be deleted."), get_selected_row()[cols.name]);
	else
		str = Glib::ustring::compose(ngettext("One action is about to be deleted.",
					"%1 actions are about to be deleted", n), n);

	Gtk::MessageDialog *dialog;
	widgets->get_widget("dialog_delete", dialog);
	dialog->set_message(ngettext("Delete an Action", "Delete Actions", n));
	dialog->set_secondary_text(str);
	Gtk::Button *del;
	widgets->get_widget("button_delete_delete", del);

	dialog->show();
	del->grab_focus();
	bool ok = dialog->run() == 1;
	dialog->hide();
	if (!ok)
		return;

	std::vector<Gtk::TreePath> paths = tv.get_selection()->get_selected_rows();
	for (std::vector<Gtk::TreePath>::iterator i = paths.begin(); i != paths.end(); ++i) {
		Gtk::TreeRow row(*tm->get_iter(*i));
		action_list->remove(row[cols.id]);
	}
	update_action_list();
	update_actions();
	update_counts();
}

void Actions::on_cell_data_apps(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter) {
	ActionListDiff *as = (*iter)[ca.actions];
	Gtk::CellRendererText *renderer = dynamic_cast<Gtk::CellRendererText *>(cell);
	if (renderer)
		renderer->property_editable().set_value(actions.get_root() != as && !as->app);
}

bool Actions::select_app(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter, ActionListDiff *actions) {
	if ((*iter)[ca.actions] == actions) {
		apps_view->expand_to_path(path);
		apps_view->set_cursor(path);
		return true;
	}
	return false;
}

void Actions::on_add_app() {
	std::string name = grabber->select_window();
	if (actions.apps.count(name)) {
		apps_model->foreach(sigc::bind(sigc::mem_fun(*this, &Actions::select_app), actions.apps[name]));
		return;
	}
	ActionListDiff *parent = action_list->app ? actions.get_root() : action_list;
	ActionListDiff *child = parent->add_child(name, true);
	const Gtk::TreeNodeChildren &ch = parent == actions.get_root() ?
		apps_model->children().begin()->children() :
		apps_view->get_selection()->get_selected()->children();
	Gtk::TreeRow row = *(apps_model->append(ch));
	row[ca.app] = app_name_hr(name);
	row[ca.actions] = child;
	actions.apps[name] = child;
	Gtk::TreePath path = apps_model->get_path(row);
	apps_view->expand_to_path(path);
	apps_view->set_cursor(path);
	update_actions();
}

void Actions::on_remove_app() {
	if (action_list == actions.get_root())
		return;
	int size = action_list->size_rec();
	if (size) {
		Gtk::MessageDialog *dialog;
		widgets->get_widget("dialog_delete", dialog);
		Glib::ustring str = Glib::ustring::compose(_("%1 \"%2\" (containing %3 %4) is about to be deleted."),
				action_list->app ? _("The application") : _("The group"),
				action_list->name,
				size,
				ngettext("action", "actions", size));
		dialog->set_message(action_list->app ? _("Delete an Application") : _("Delete an Application Group"));
		dialog->set_secondary_text(str);
		Gtk::Button *del;
		widgets->get_widget("button_delete_delete", del);
		dialog->show();
		del->grab_focus();
		bool ok = dialog->run() == 1;
		dialog->hide();
		if (!ok)
			return;
	}
	if (!action_list->remove())
		return;
	apps_model->erase(*apps_view->get_selection()->get_selected());
	update_actions();
}

void Actions::on_reset_actions() {
	std::vector<Gtk::TreePath> paths = tv.get_selection()->get_selected_rows();
	for (std::vector<Gtk::TreePath>::iterator i = paths.begin(); i != paths.end(); ++i) {
		Gtk::TreeRow row(*tm->get_iter(*i));
		action_list->reset(row[cols.id]);
	}
	update_action_list();
	on_selection_changed();
	update_actions();
}

void Actions::on_add_group() {
	ActionListDiff *parent = action_list->app ? actions.get_root() : action_list;
	Glib::ustring name = _("Group");
	ActionListDiff *child = parent->add_child(name, false);
	const Gtk::TreeNodeChildren &ch = parent == actions.get_root() ?
		apps_model->children().begin()->children() :
		apps_view->get_selection()->get_selected()->children();
	Gtk::TreeRow row = *(apps_model->append(ch));
	row[ca.app] = name;
	row[ca.actions] = child;
	actions.apps[name] = child;
	Gtk::TreePath path = apps_model->get_path(row);
	apps_view->expand_to_path(path);
	apps_view->set_cursor(path, *apps_view->get_column(0), true);
	update_actions();
}

void Actions::on_group_name_edited(const Glib::ustring& path, const Glib::ustring& new_text) {
	Gtk::TreeRow row(*apps_model->get_iter(path));
	row[ca.app] = new_text;
	ActionListDiff *as = row[ca.actions];
	as->name = new_text;
	update_actions();
}

void Actions::on_expanded() {
	if (expander_apps->get_expanded()) {
		vpaned_apps->set_position(vpaned_position);
	} else {
		if(vpaned_apps->property_position_set().get_value())
			vpaned_position = vpaned_apps->get_position();
		else
			vpaned_position = -1;
		vpaned_apps->property_position_set().set_value(false);
	}
}

void Actions::on_apps_selection_changed() {
	ActionListDiff *new_action_list = actions.get_root();
	if (expander_apps->property_expanded().get_value()) {
		if (apps_view->get_selection()->count_selected_rows()) {
			Gtk::TreeIter i = apps_view->get_selection()->get_selected();
			new_action_list = (*i)[ca.actions];
		}
		button_remove_app->set_sensitive(new_action_list != actions.get_root());
	}
	if (action_list != new_action_list) {
		action_list = new_action_list;
		update_action_list();
		on_selection_changed();
	}
}

bool Actions::count_app_actions(const Gtk::TreeIter &i) {
	(*i)[ca.count] = ((ActionListDiff*)(*i)[ca.actions])->count_actions();
	return false;
}

void Actions::update_counts() {
	apps_model->foreach_iter(sigc::mem_fun(*this, &Actions::count_app_actions));
}

void Actions::update_action_list() {
	check_show_deleted->set_sensitive(action_list != actions.get_root());
	boost::shared_ptr<std::set<Unique *> > ids = action_list->get_ids(check_show_deleted->get_active());
	const Gtk::TreeNodeChildren &ch = tm->children();

	std::list<Gtk::TreeRowReference> refs;
	for (Gtk::TreeIter i = ch.begin(); i != ch.end(); i++) {
		Gtk::TreeRowReference ref(tm, Gtk::TreePath(*i));
		refs.push_back(ref);
	}

	for (std::list<Gtk::TreeRowReference>::iterator ref = refs.begin(); ref != refs.end(); ref++) {
		Gtk::TreeIter i = tm->get_iter(ref->get_path());
		std::set<Unique *>::iterator id = ids->find((*i)[cols.id]);
		if (id == ids->end()) {
			tm->erase(i);
		} else {
			ids->erase(id);
			update_row(*i);
		}
	}
	for (std::set<Unique *>::const_iterator i = ids->begin(); i != ids->end(); i++) {
		Gtk::TreeRow row = *tm->append();
		row[cols.id] = *i;
		update_row(row);
	}
}

void Actions::update_row(const Gtk::TreeRow &row) {
	bool deleted, stroke, name, action;
	RStrokeInfo si = action_list->get_info(row[cols.id], &deleted, &stroke, &name, &action);
	row[cols.stroke] = !si->strokes.empty() && *si->strokes.begin() ? 
		(*si->strokes.begin())->draw(STROKE_SIZE, stroke ? 4.0 : 2.0) : Stroke::drawEmpty(STROKE_SIZE);
	row[cols.name] = si->name;
	row[cols.type] = si->action ? type_info_to_name(&typeid(*si->action)) : "";
	row[cols.arg]  = si->action ? si->action->get_label() : "";
	row[cols.deactivated] = deleted;
	row[cols.name_bold] = name;
	row[cols.action_bold] = action;
}

extern boost::shared_ptr<sigc::slot<void, RStroke> > stroke_action;
Source<bool> recording(false);

class Actions::OnStroke {
	Actions *parent;
	Gtk::Dialog *dialog;
	Gtk::TreeRow &row;
	RStroke stroke;
	bool run() {
		if (stroke->button == 0 && stroke->trivial()) {
			grabber->queue_suspend();
			Glib::ustring msg = Glib::ustring::compose(
					_("You are about to bind an action to a single click.  "
						"This might make it difficult to use Button %1 in the future.  "
						"Are you sure you want to continue?"),
					stroke->button ? stroke->button : prefs.button.ref().button);
			Gtk::MessageDialog md(*dialog, msg, false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);
			bool abort = md.run() != Gtk::RESPONSE_YES;
			grabber->queue_resume();
			if (abort)
				return false;
		}
		StrokeSet strokes;
		strokes.insert(stroke);
		parent->action_list->set_strokes(row[parent->cols.id], strokes);
		parent->update_row(row);
		parent->on_selection_changed();
		update_actions();
		dialog->response(0);
		return false;
	}
public:
	OnStroke(Actions *parent_, Gtk::Dialog *dialog_, Gtk::TreeRow &row_) : parent(parent_), dialog(dialog_), row(row_) {}
	void delayed_run(RStroke stroke_) {
		stroke = stroke_;
		Glib::signal_idle().connect(sigc::mem_fun(*this, &OnStroke::run));
		stroke_action.reset();
		recording.set(false);
	}
};

void Actions::on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column) {
	Gtk::TreeRow row(*tm->get_iter(path));
	Gtk::MessageDialog *dialog;
	widgets->get_widget("dialog_record", dialog);
	dialog->set_message(_("Record a New Stroke"));
	dialog->set_secondary_text(Glib::ustring::compose(_("The next stroke will be associated with the action \"%1\".  You can draw it anywhere on the screen (except for the two buttons below)."), row[cols.name]));

	static Gtk::Button *del = 0, *cancel = 0;
	if (!del) {
		widgets->get_widget("button_record_delete", del);
		del->signal_enter().connect(sigc::mem_fun(*grabber, &Grabber::queue_suspend));
		del->signal_leave().connect(sigc::mem_fun(*grabber, &Grabber::queue_resume));
	}
	if (!cancel) {
		widgets->get_widget("button_record_cancel", cancel);
		cancel->signal_enter().connect(sigc::mem_fun(*grabber, &Grabber::queue_suspend));
		cancel->signal_leave().connect(sigc::mem_fun(*grabber, &Grabber::queue_resume));
	}
	RStrokeInfo si = action_list->get_info(row[cols.id]);
	if (si)
		del->set_sensitive(si->strokes.size() != 0);

	OnStroke ps(this, dialog, row);
	stroke_action.reset(new sigc::slot<void, RStroke>(sigc::mem_fun(ps, &OnStroke::delayed_run)));
	recording.set(true);

	dialog->show();
	cancel->grab_focus();
	int response = dialog->run();
	dialog->hide();
	stroke_action.reset();
	recording.set(false);
	if (response != 1)
		return;

	action_list->set_strokes(row[cols.id], StrokeSet());
	update_row(row);
	on_selection_changed();
	update_actions();
}

void Actions::on_button_record() {
	Gtk::TreeModel::Path path;
	Gtk::TreeViewColumn *col;
	tv.get_cursor(path, col);
	on_row_activated(path, col);
}

Gtk::TreeRow Actions::get_selected_row() {
	std::vector<Gtk::TreePath> paths = tv.get_selection()->get_selected_rows();
	return Gtk::TreeRow(*tm->get_iter(*paths.begin()));
}

void Actions::on_selection_changed() {
	int n = tv.get_selection()->count_selected_rows();
	button_record->set_sensitive(n == 1);
	button_delete->set_sensitive(n >= 1);
	bool resettable = false;
	if (n) {
		std::vector<Gtk::TreePath> paths = tv.get_selection()->get_selected_rows();
		for (std::vector<Gtk::TreePath>::iterator i = paths.begin(); i != paths.end(); ++i) {
			Gtk::TreeRow row(*tm->get_iter(*i));
			if (action_list->resettable(row[cols.id])) {
				resettable = true;
				break;
			}
		}
	}
	button_reset_actions->set_sensitive(resettable);
}

void Actions::on_button_new() {
	editing_new = true;
	Unique *before = 0;
	if (tv.get_selection()->count_selected_rows()) {
		std::vector<Gtk::TreePath> paths = tv.get_selection()->get_selected_rows();
		Gtk::TreeIter i = tm->get_iter(paths[paths.size()-1]);
		i++;
		if (i != tm->children().end())
			before = (*i)[cols.id];
	}

	Gtk::TreeModel::Row row = *(tm->append());
	StrokeInfo si;
	si.action = Command::create("");
	Unique *id = action_list->add(si, before);
	row[cols.id] = id;
	std::string name;
	if (action_list != actions.get_root())
		name = action_list->name + " ";
	name += Glib::ustring::compose(_("Gesture %1"), action_list->order_size());
	action_list->set_name(id, name);

	update_row(row);
	focus(id, 1, true);
	update_actions();
	update_counts();
}

bool Actions::do_focus(Unique *id, Gtk::TreeViewColumn *col, bool edit) {
	if (!editing) {
		Gtk::TreeModel::Children chs = tm->children();
		for (Gtk::TreeIter i = chs.begin(); i != chs.end(); ++i)
			if ((*i)[cols.id] == id) {
				tv.set_cursor(Gtk::TreePath(*i), *col, edit);
			}
	}
	return false;
}

void Actions::focus(Unique *id, int col, bool edit) {
	editing = false;
	Glib::signal_idle().connect(sigc::bind(sigc::mem_fun(*this, &Actions::do_focus), id, tv.get_column(col), edit));
}

void Actions::on_name_edited(const Glib::ustring& path, const Glib::ustring& new_text) {
	Gtk::TreeRow row(*tm->get_iter(path));
	action_list->set_name(row[cols.id], new_text);
	update_actions();
	update_row(row);
	focus(row[cols.id], 2, editing_new);
}

void Actions::on_text_edited(const gchar *path, const gchar *new_text) {
	Gtk::TreeRow row(*tm->get_iter(path));
	Type type = from_name(row[cols.type]);
	if (type == COMMAND) {
		action_list->set_action(row[cols.id], Command::create(new_text));
	} else if (type == TEXT) {
		action_list->set_action(row[cols.id], SendText::create(new_text));
	} else return;
	update_row(row);
	update_actions();
}

void Actions::on_accel_edited(const gchar *path_string, guint accel_key, GdkModifierType mods, guint hardware_keycode) {
	Gdk::ModifierType accel_mods = (Gdk::ModifierType)mods;
	Gtk::TreeRow row(*tm->get_iter(path_string));
	Type type = from_name(row[cols.type]);
	if (type == KEY) {
		RSendKey send_key = SendKey::create(accel_key, accel_mods);
		Glib::ustring str = send_key->get_label();
		if (row[cols.arg] == str)
			return;
		action_list->set_action(row[cols.id], boost::static_pointer_cast<Action>(send_key));
	} else if (type == SCROLL) {
		RScroll scroll = Scroll::create(accel_mods);
		Glib::ustring str = scroll->get_label();
		if (row[cols.arg] == str)
			return;
		action_list->set_action(row[cols.id], boost::static_pointer_cast<Action>(scroll));
	} else if (type == IGNORE) {
		RIgnore ignore = Ignore::create(accel_mods);
		Glib::ustring str = ignore->get_label();
		if (row[cols.arg] == str)
			return;
		action_list->set_action(row[cols.id], boost::static_pointer_cast<Action>(ignore));
	} else return;
	update_row(row);
	update_actions();
}

void Actions::on_combo_edited(const gchar *path_string, guint item) {
	RMisc misc = Misc::create((Misc::Type)item);
	Glib::ustring str = misc->get_label();
	Gtk::TreeRow row(*tm->get_iter(path_string));
	if (row[cols.arg] == str)
		return;
	action_list->set_action(row[cols.id], boost::static_pointer_cast<Action>(misc));
	update_row(row);
	update_actions();
}

void Actions::on_something_editing_canceled() {
	editing_new = false;
}

void Actions::on_something_editing_started(Gtk::CellEditable* editable, const Glib::ustring& path) {
	editing = true;
}

void Actions::on_arg_editing_started(GtkCellEditable *editable, const gchar *path) {
	tv.grab_focus();
	Gtk::TreeRow row(*tm->get_iter(path));
	if (from_name(row[cols.type]) != BUTTON)
		return;
	ButtonInfo bi;
	RButton bt = boost::static_pointer_cast<Button>(action_list->get_info(row[cols.id])->action);
	if (bt)
		bi = bt->get_button_info();
	SelectButton sb(bi, false, false);
	if (!sb.run())
		return;
	bt = boost::static_pointer_cast<Button>(Button::create(Gdk::ModifierType(sb.event.state), sb.event.button));
	action_list->set_action(row[cols.id], bt);
	update_row(row);
	update_actions();
}

const Glib::ustring SendKey::get_label() const {
	return Gtk::AccelGroup::get_label(key, mods);
}

const Glib::ustring ModAction::get_label() const {
	if (!mods)
		return _("No Modifiers");
	Glib::ustring label = Gtk::AccelGroup::get_label(0, mods);
	return label.substr(0,label.size()-1);
}

Glib::ustring ButtonInfo::get_button_text() const {
	Glib::ustring str;
	if (instant)
		str += _("(Instantly) ");
	if (click_hold)
		str += _("(Click & Hold) ");
	if (state == AnyModifier)
		str += Glib::ustring() + "(" + _("Any Modifier") + " +) ";
	else
		str += Gtk::AccelGroup::get_label(0, (Gdk::ModifierType)state);
	return str + Glib::ustring::compose(_("Button %1"), button);
}

const Glib::ustring Scroll::get_label() const {
	if (mods)
		return ModAction::get_label() + _(" + Scroll");
	else
		return _("Scroll");
}

const Glib::ustring Ignore::get_label() const {
	if (mods)
		return ModAction::get_label();
	else
		return _("Ignore");
}
