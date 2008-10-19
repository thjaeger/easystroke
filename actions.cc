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
#include "actions.h"
#include "actiondb.h"
#include "win.h"
#include "main.h"
#include "prefdb.h"

#include <iostream>
#include <sstream>
#include <typeinfo>

class CellEditableAccel : public Gtk::EventBox, public Gtk::CellEditable {
	CellRendererTextish *parent;
	Glib::ustring path;
public:
	CellEditableAccel(CellRendererTextish *parent_, const Glib::ustring &path_, Gtk::Widget &widget) :
		Glib::ObjectBase(typeid(CellEditableAccel)),
		parent(parent_), path(path_)
	{
		WIDGET(Gtk::Label, label, "Key combination...");
		label.set_alignment(0.0, 0.5);
		add(label);
		modify_bg(Gtk::STATE_NORMAL, widget.get_style()->get_bg(Gtk::STATE_SELECTED));
		label.modify_fg(Gtk::STATE_NORMAL, widget.get_style()->get_fg(Gtk::STATE_SELECTED));
		show_all();
	}
protected:

	virtual void start_editing_vfunc(GdkEvent *event) {
		add_modal_grab();
		get_window()->keyboard_grab(false, gdk_event_get_time(event));
		signal_key_press_event().connect(sigc::mem_fun(*this, &CellEditableAccel::on_key));
	}

	bool on_key(GdkEventKey* event) {
		if (event->is_modifier)
			return true;
		switch (event->keyval) {
			case GDK_Super_L:
			case GDK_Super_R:
			case GDK_Hyper_L:
			case GDK_Hyper_R:
				return true;
		}
		guint key = gdk_keyval_to_lower(event->keyval);
		guint mods = event->state & gtk_accelerator_get_default_mod_mask();

		editing_done();
		remove_widget();

		parent->signal_key_edited().emit(path, key, (Gdk::ModifierType)mods, event->hardware_keycode);
		return true;
	}

	virtual void on_editing_done() {
		remove_modal_grab();
		get_window()->keyboard_ungrab(CurrentTime);
		Gtk::CellEditable::on_editing_done();
	}
};

class CellEditableCombo : public Gtk::ComboBoxText, public Gtk::CellEditable {
	CellRendererTextish *parent;
	Glib::ustring path;
public:
	CellEditableCombo(CellRendererTextish *parent_, const Glib::ustring &path_, Gtk::Widget &widget, const char **items) :
		Glib::ObjectBase(typeid(CellEditableCombo)),
		parent(parent_), path(path_)
	{
		while (*items)
			append_text(*(items++));
	}
protected:
	virtual void on_changed() {
		parent->signal_combo_edited().emit(path, get_active_row_number());
	}
};

Gtk::CellEditable* CellRendererTextish::start_editing_vfunc(GdkEvent *event, Gtk::Widget &widget, const Glib::ustring &path,
		const Gdk::Rectangle &background_area, const Gdk::Rectangle &cell_area, Gtk::CellRendererState flags) {
	if (!property_editable())
		    return 0;
	if (mode == TEXT)
		return Gtk::CellRendererText::start_editing_vfunc(event, widget, path, background_area, cell_area, flags);
	if (mode == KEY) {
		return Gtk::manage(new CellEditableAccel(this, path, widget));
	}
	if (mode == COMBO) {
		return Gtk::manage(new CellEditableCombo(this, path, widget, items));
	}
	return 0;
}

const char *KEY = "Key";
const char *COMMAND = "Command";
const char *SCROLL = "Scroll";
const char *IGNORE = "Ignore";
const char *BUTTON = "Button";
const char *MISC = "Misc";

struct NameType {
	const char *name;
	const std::type_info *type;
};

NameType name_type[7] = {
	{ COMMAND, &typeid(Command) },
	{ KEY,     &typeid(SendKey) },
	{ SCROLL,  &typeid(Scroll)  },
	{ IGNORE,  &typeid(Ignore)  },
	{ BUTTON,  &typeid(Button)  },
	{ MISC,    &typeid(Misc)    },
	{ 0,       0                }
};

const std::type_info *name_to_type(Glib::ustring name) {
	for (NameType *i = name_type; i->name; i++)
		if (i->name == name)
			return i->type;
	return 0;
}

const char *type_to_name(const std::type_info *type) {
	for (NameType *i = name_type; i->name; i++)
		if (i->type == type)
			return i->name;
	return "";
}

Actions::Actions() :
	tv(0),
	apps_view(0),
	editing_new(false),
	editing(false),
	action_list(actions.get_root())
{
	widgets->get_widget("treeview_actions", tv);
	widgets->get_widget("treeview_apps", apps_view);

	Gtk::Button *button_add, *button_add_app, *button_add_group, *button_reset_actions;
	widgets->get_widget("button_add_action", button_add);
	widgets->get_widget("button_delete_action", button_delete);
	widgets->get_widget("button_record", button_record);
	widgets->get_widget("button_add_app", button_add_app);
	widgets->get_widget("button_add_group", button_add_group);
	widgets->get_widget("button_remove_app", button_remove_app);
	widgets->get_widget("button_reset_actions", button_reset_actions);
	widgets->get_widget("check_show_deleted", check_show_deleted);
	widgets->get_widget("expander_apps", expander_apps);
	button_record->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_record));
	button_delete->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_delete));
	button_add->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_new));
	button_add_app->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_add_app));
	button_add_group->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_add_group));
	button_remove_app->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_remove_app));
	button_reset_actions->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_reset_actions));

	tv->signal_cursor_changed().connect(sigc::mem_fun(*this, &Actions::on_cursor_changed));
	tv->signal_row_activated().connect(sigc::mem_fun(*this, &Actions::on_row_activated));
	tv->get_selection()->signal_changed().connect(sigc::mem_fun(*this, &Actions::on_selection_changed));

	tm = Gtk::ListStore::create(cols);

	tv->get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);

	int n;
	tv->append_column("Stroke", cols.stroke);

	n = tv->append_column("Name", cols.name);
	Gtk::CellRendererText *name_renderer = dynamic_cast<Gtk::CellRendererText *>(tv->get_column_cell_renderer(n-1));
	name_renderer->property_editable() = true;
	name_renderer->signal_edited().connect(sigc::mem_fun(*this, &Actions::on_name_edited));
	name_renderer->signal_editing_started().connect(sigc::mem_fun(*this, &Actions::on_something_editing_started));
	name_renderer->signal_editing_canceled().connect(sigc::mem_fun(*this, &Actions::on_something_editing_canceled));
	Gtk::TreeView::Column *col_name = tv->get_column(n-1);
	col_name->set_sort_column(cols.name);
//	col_name->add_attribute(name_renderer->property_text(), cols.name);
	col_name->set_cell_data_func(*name_renderer, sigc::mem_fun(*this, &Actions::on_cell_data_name));

	type_store = Gtk::ListStore::create(type);
	for (NameType *i = name_type; i->name; i++)
		(*(type_store->append()))[type.type] = i->name;

	Gtk::CellRendererCombo *type_renderer = Gtk::manage(new Gtk::CellRendererCombo);
	type_renderer->property_model() = type_store;
	type_renderer->property_editable() = true;
	type_renderer->property_text_column() = 0;
	type_renderer->property_has_entry() = false;
	type_renderer->signal_edited().connect(sigc::mem_fun(*this, &Actions::on_type_edited));
	type_renderer->signal_editing_started().connect(sigc::mem_fun(*this, &Actions::on_something_editing_started));
	type_renderer->signal_editing_canceled().connect(sigc::mem_fun(*this, &Actions::on_something_editing_canceled));

	n = tv->append_column("Type", *type_renderer);
	Gtk::TreeView::Column *col_type = tv->get_column(n-1);
	col_type->add_attribute(type_renderer->property_text(), cols.type);
	col_type->set_cell_data_func(*type_renderer, sigc::mem_fun(*this, &Actions::on_cell_data_type));

	CellRendererTextish *arg_renderer = Gtk::manage(new CellRendererTextish);
	n = tv->append_column("Argument", *arg_renderer);
	Gtk::TreeView::Column *col_arg = tv->get_column(n-1);
	col_arg->add_attribute(arg_renderer->property_text(), cols.arg);
	col_arg->set_cell_data_func(*arg_renderer, sigc::mem_fun(*this, &Actions::on_cell_data_arg));
	arg_renderer->property_editable() = true;
	arg_renderer->signal_key_edited().connect(sigc::mem_fun(*this, &Actions::on_accel_edited));
	arg_renderer->signal_combo_edited().connect(sigc::mem_fun(*this, &Actions::on_combo_edited));
	arg_renderer->signal_edited().connect(sigc::mem_fun(*this, &Actions::on_cmd_edited));
	arg_renderer->signal_editing_started().connect(sigc::mem_fun(*this, &Actions::on_arg_editing_started));

	update_action_list();
	tv->set_model(tm);

	check_show_deleted->signal_toggled().connect(sigc::mem_fun(*this, &Actions::update_action_list));
	expander_apps->property_expanded().signal_changed().connect(sigc::mem_fun(*this, &Actions::on_apps_selection_changed));
	apps_view->get_selection()->signal_changed().connect(sigc::mem_fun(*this, &Actions::on_apps_selection_changed));
	apps_model = Gtk::TreeStore::create(ca);

	load_app_list(apps_model->children(), actions.get_root());

	apps_view->append_column_editable("Application", ca.app);
	apps_view->get_column(0)->set_cell_data_func(
			*apps_view->get_column_cell_renderer(0), sigc::mem_fun(*this, &Actions::on_cell_data_apps));
	Gtk::CellRendererText *app_name_renderer =
		dynamic_cast<Gtk::CellRendererText *>(apps_view->get_column_cell_renderer(0));
	app_name_renderer->signal_edited().connect(sigc::mem_fun(*this, &Actions::on_group_name_edited));

	apps_view->set_model(apps_model);
	apps_view->expand_all();
}

void Actions::load_app_list(const Gtk::TreeNodeChildren &ch, ActionListDiff *actions) {
	Gtk::TreeRow row = *(apps_model->append(ch));
	row[ca.app] = actions->name;
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

void Actions::on_cell_data_arg(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter) {
	bool bold = (*iter)[cols.action_bold];
	bool deactivated = (*iter)[cols.deactivated];
	cell->property_sensitive().set_value(!deactivated);
	CellRendererTextish *renderer = dynamic_cast<CellRendererTextish *>(cell);
	if (!renderer)
		return;
	renderer->property_weight().set_value(bold ? 700 : 400);

	Glib::ustring str = (*iter)[cols.type];
	if (str == KEY || str == SCROLL || str == IGNORE)
		renderer->mode = CellRendererTextish::KEY;
	else if (str == BUTTON)
		renderer->mode = CellRendererTextish::POPUP;
	else if (str == MISC) {
		renderer->mode = CellRendererTextish::COMBO;
		renderer->items = Misc::types;
	} else
		renderer->mode = CellRendererTextish::TEXT;
}

void Actions::on_type_edited(const Glib::ustring& path, const Glib::ustring& new_type) {
	tv->grab_focus();
	Gtk::TreeRow row(*tm->get_iter(path));
	Glib::ustring old_type = row[cols.type];
	bool edit = true;
	if (old_type == new_type) {
		edit = editing_new;
	} else {
		row[cols.type] = new_type;
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
			new_action = SendKey::create(0, (Gdk::ModifierType)0, 0);
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
	int n = tv->get_selection()->count_selected_rows();

	std::stringstream msg;
	if (n == 1)
		msg << "Action \"" << (*tv->get_selection()->get_selected())[cols.name] << "\" is";
	else
		msg << n << " actions are";

	Gtk::Dialog *dialog;
	widgets->get_widget("dialog_delete", dialog);
	FormatLabel foo(widgets, "label_delete", msg.str().c_str());

	bool ok = dialog->run() == 1;
	dialog->hide();
	if (!ok)
		return;

	std::vector<Gtk::TreePath> paths = tv->get_selection()->get_selected_rows();
	for (std::vector<Gtk::TreePath>::iterator i = paths.begin(); i != paths.end(); ++i) {
		Gtk::TreeRow row(*tm->get_iter(*i));
		action_list->remove(row[cols.id]);
	}
	update_action_list();
	update_actions();
}

void Actions::on_cell_data_apps(Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& iter) {
	ActionListDiff *as = (*iter)[ca.actions];
	Gtk::CellRendererText *renderer = dynamic_cast<Gtk::CellRendererText *>(cell);
	if (renderer)
		renderer->property_editable().set_value(actions.get_root() != as && !as->app);
}

void select_window(sigc::slot<void, std::string> f);

void Actions::on_add_app() {
	select_window(sigc::mem_fun(*this, &Actions::on_app_selected));
}

void Actions::on_remove_app() {
	if (!action_list->remove())
		return;
	apps_model->erase(*apps_view->get_selection()->get_selected());
	update_actions();
}

void Actions::on_reset_actions() {
	std::vector<Gtk::TreePath> paths = tv->get_selection()->get_selected_rows();
	for (std::vector<Gtk::TreePath>::iterator i = paths.begin(); i != paths.end(); ++i) {
		Gtk::TreeRow row(*tm->get_iter(*i));
		action_list->reset(row[cols.id]);
//		update_row(row); // This can't handle deleted rows...
	}
	update_action_list();
}

struct Actions::SelectApp {
	Actions *parent;
	ActionListDiff *actions;
	bool test(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter) {
		if ((*iter)[parent->ca.actions] == actions) {
			parent->apps_view->expand_to_path(path);
			parent->apps_view->set_cursor(path);
			return true;
		}
		return false;
	}
};

void Actions::on_app_selected(std::string name) {
	if (actions.apps.count(name)) {
		SelectApp cb;
		cb.parent = this;
		cb.actions = actions.apps[name];
		apps_model->foreach(sigc::mem_fun(cb, &SelectApp::test));
		return;
	}
	ActionListDiff *parent = action_list->app ? actions.get_root() : action_list;
	ActionListDiff *child = parent->add_child(name, true);
	const Gtk::TreeNodeChildren &ch = parent == actions.get_root() ?
	       	apps_model->children().begin()->children() :
		apps_view->get_selection()->get_selected()->children();
	Gtk::TreeRow row = *(apps_model->append(ch));
	row[ca.app] = name;
	row[ca.actions] = child;
	actions.apps[name] = child;
	Gtk::TreePath path = apps_model->get_path(row);
	apps_view->expand_to_path(path);
	apps_view->set_cursor(path);
	update_actions();
}

void Actions::on_add_group() {
	ActionListDiff *parent = action_list->app ? actions.get_root() : action_list;
	Glib::ustring name = "Group 0"; //TODO
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
	}
}

void Actions::update_action_list() {
	boost::shared_ptr<std::set<Unique *> > ids = action_list->get_ids(check_show_deleted->get_active());
	const Gtk::TreeNodeChildren &ch = tm->children();
	for (Gtk::TreeIter i = ch.begin(); i != ch.end(); i++) {
		std::set<Unique *>::iterator id = ids->find((*i)[cols.id]);
		if (id == ids->end()) {
			tm->erase(*i);
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
		(*si->strokes.begin())->draw(STROKE_SIZE, stroke) : Stroke::drawEmpty(STROKE_SIZE);
	row[cols.name] = si->name;
	row[cols.type] = si->action ? type_to_name(&typeid(*si->action)) : "";
	row[cols.arg]  = si->action ? si->action->get_label() : "";
	row[cols.deactivated] = deleted;
	row[cols.name_bold] = name;
	row[cols.action_bold] = action;
}

class Actions::OnStroke {
	Actions *parent;
	Gtk::Dialog *dialog;
	Gtk::TreeRow &row;
public:
	OnStroke(Actions *parent_, Gtk::Dialog *dialog_, Gtk::TreeRow &row_) : parent(parent_), dialog(dialog_), row(row_) {}
	void run(RStroke stroke) {
		StrokeSet strokes;
		strokes.insert(stroke);
		parent->action_list->set_strokes(row[parent->cols.id], strokes);
		parent->update_row(row);
		update_actions();
		dialog->response(0);
	}
};

extern boost::shared_ptr<sigc::slot<void, RStroke> > stroke_action;
void suspend_flush();
void resume_flush();

void Actions::on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column) {
	Gtk::Dialog *dialog;
	widgets->get_widget("dialog_record", dialog);
	Gtk::TreeRow row(*tm->get_iter(path));
	FormatLabel foo(widgets, "label_record", Glib::ustring(row[cols.name]).c_str());

	static Gtk::Button *del = 0, *cancel = 0;
	if (!del) {
		widgets->get_widget("button_record_delete", del);
		del->signal_enter().connect(sigc::ptr_fun(&suspend_flush));
		del->signal_leave().connect(sigc::ptr_fun(&resume_flush));
	}
	if (!cancel) {
		widgets->get_widget("button_record_cancel", cancel);
		cancel->signal_enter().connect(sigc::ptr_fun(&suspend_flush));
		cancel->signal_leave().connect(sigc::ptr_fun(&resume_flush));
	}
	RStrokeInfo si = action_list->get_info(row[cols.id]);
	if (si)
		del->set_sensitive(si->strokes.size());

	OnStroke ps(this, dialog, row);
	stroke_action.reset(new sigc::slot<void, RStroke>(sigc::mem_fun(ps, &OnStroke::run)));

	int response = dialog->run();
	dialog->hide();
	stroke_action.reset();
	if (response != 1)
		return;

	action_list->set_strokes(row[cols.id], StrokeSet());
	update_row(row);
	update_actions();
}

void Actions::on_button_record() {
	Gtk::TreeModel::Path path;
	Gtk::TreeViewColumn *col;
	tv->get_cursor(path, col);
	on_row_activated(path, col);
}

void Actions::on_cursor_changed() {
	Gtk::TreeModel::Path path;
	Gtk::TreeViewColumn *col;
	tv->get_cursor(path, col);
	Gtk::TreeRow row(*tm->get_iter(path));
}

void Actions::on_selection_changed() {
	int n = tv->get_selection()->count_selected_rows();
	button_record->set_sensitive(n == 1);
	button_delete->set_sensitive(n >= 1);
}

void Actions::on_button_new() {
	editing_new = true;

	Gtk::TreeModel::Row row = *(tm->append());
	char buf[16];
	snprintf(buf, 15, "Gesture %d", 0); // TODO: actions.ref().size()+1);
	StrokeInfo si;
	si.name = buf;
	si.action = Command::create("");
	row[cols.id] = action_list->add(si);

	update_row(row);
	focus(row[cols.id], 1, true);
	update_actions();
}

struct Actions::Focus {
	Actions *parent;
	Unique *id;
	Gtk::TreeViewColumn* col;
	bool edit;
	bool focus() {
		if (!parent->editing) {
			Gtk::TreeModel::Children chs = parent->tm->children();
			for (Gtk::TreeIter i = chs.begin(); i != chs.end(); ++i)
				if ((*i)[parent->cols.id] == id) {
					parent->tv->set_cursor(Gtk::TreePath(*i), *col, edit);
				}
		}
		delete this;
		return false;
	}
};

void Actions::focus(Unique *id, int col, bool edit) {
	// More C++ closure fun.
	Focus* focus = new Focus;
	focus->parent = this;
	focus->id = id;
	focus->col = tv->get_column(col);
	focus->edit = edit;
	editing = false;
	Glib::signal_idle().connect(sigc::mem_fun(*focus, &Focus::focus));
}

void Actions::on_name_edited(const Glib::ustring& path, const Glib::ustring& new_text) {
	Gtk::TreeRow row(*tm->get_iter(path));
	action_list->set_name(row[cols.id], new_text);
	update_actions();
	update_row(row);
	focus(row[cols.id], 2, editing_new);
}

void Actions::on_cmd_edited(const Glib::ustring& path, const Glib::ustring& new_cmd) {
	Gtk::TreeRow row(*tm->get_iter(path));
	action_list->set_action(row[cols.id], Command::create(new_cmd));
	update_row(row);
	update_actions();
}

void Actions::on_accel_edited(const Glib::ustring& path_string, guint accel_key, Gdk::ModifierType accel_mods, guint hardware_keycode) {
	Gtk::TreeRow row(*tm->get_iter(path_string));
	if (row[cols.type] == KEY) {
		RSendKey send_key = SendKey::create(accel_key, accel_mods, hardware_keycode);
		Glib::ustring str = send_key->get_label();
		if (row[cols.arg] == str)
			return;
		action_list->set_action(row[cols.id], boost::static_pointer_cast<Action>(send_key));
	}
	if (row[cols.type] == SCROLL) {
		RScroll scroll = Scroll::create(accel_mods);
		Glib::ustring str = scroll->get_label();
		if (row[cols.arg] == str)
			return;
		action_list->set_action(row[cols.id], boost::static_pointer_cast<Action>(scroll));
	}
	if (row[cols.type] == IGNORE) {
		RIgnore ignore = Ignore::create(accel_mods);
		Glib::ustring str = ignore->get_label();
		if (row[cols.arg] == str)
			return;
		action_list->set_action(row[cols.id], boost::static_pointer_cast<Action>(ignore));
	}
	update_row(row);
	update_actions();
}

void Actions::on_combo_edited(const Glib::ustring& path_string, guint item) {
	if (item < 0)
		item = 0;
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

void Actions::on_arg_editing_started(Gtk::CellEditable* editable, const Glib::ustring& path) {
	tv->grab_focus();
	Gtk::TreeRow row(*tm->get_iter(path));
	if (row[cols.type] != Glib::ustring(BUTTON))
		return;
	ButtonInfo bi;
	RButton bt = boost::static_pointer_cast<Button>(action_list->get_info(row[cols.id])->action);
	if (bt)
		bi = bt->get_button_info();
	SelectButton sb(bi, false);
	if (!sb.run())
		return;
	bt = boost::static_pointer_cast<Button>(Button::create(Gdk::ModifierType(sb.event.state), sb.event.button));
	action_list->set_action(row[cols.id], bt);
	update_row(row);
	update_actions();
}

const Glib::ustring SendKey::get_label() const {
	Glib::ustring str = Gtk::AccelGroup::get_label(key, mods);
	if (key == 0) {
		char buf[10];
		snprintf(buf, 9, "0x%x", code);
		str += buf;
	}
	return str;
}

const Glib::ustring ModAction::get_label() const {
	if (!mods)
		return "No Modifiers";
	Glib::ustring label = Gtk::AccelGroup::get_label(0, mods);
	return label.substr(0,label.size()-1);
}

Glib::ustring ButtonInfo::get_button_text() const {
	Glib::ustring str = Gtk::AccelGroup::get_label(0, (Gdk::ModifierType)state);
	char name[16];
	snprintf(name, 15, "Button %d", button);
	return str + name;
}
