#include "actions.h"
#include "actiondb.h"
#include "win.h"
#include "strokeaction.h"
#include "main.h"
#include "prefdb.h"

#include <iostream>
#include <sstream>

#define KEY "Key (SendKey)"
#define KEY_XTEST "Key"
#define COMMAND "Command"
#define SCROLL "Scroll"
#define IGNORE "Ignore"
#define BUTTON "Button"

Actions::Actions() :
	good_state(true),
	tv(0),
	editing_new(false),
	editing(false)
{
	widgets->get_widget("treeview_actions", tv);

	Gtk::Button *button_add = 0;
	widgets->get_widget("button_add_action", button_add);
	widgets->get_widget("button_delete_action", button_delete);
	widgets->get_widget("button_record", button_record);
	button_record->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_record));
	button_delete->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_delete));
	button_add->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_new));

	tv->signal_cursor_changed().connect(sigc::mem_fun(*this, &Actions::on_cursor_changed));
	tv->signal_row_activated().connect(sigc::mem_fun(*this, &Actions::on_row_activated));
	tv->get_selection()->signal_changed().connect(sigc::mem_fun(*this, &Actions::on_selection_changed));

	tm = Gtk::TreeStore::create(cols);

	tv->get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);

	Gtk::TreeModel::Row row;
	{
		Setter s;
		const ActionDB &as = s.ref(actions);
		for (ActionDB::const_iterator i = as.begin(); i!=as.end(); i++) {
			const StrokeInfo &si = i->second;
			row = *(tm->append());
			row[cols.stroke] = !si.strokes.empty() && *si.strokes.begin() ? (*si.strokes.begin())->draw(STROKE_SIZE) : Stroke::drawEmpty(STROKE_SIZE);
			row[cols.name] = i->second.name;
			row[cols.type] = COMMAND;
			row[cols.id]   = i->first;

			RCommand cmd = boost::dynamic_pointer_cast<Command>(si.action);
			if (cmd)
				row[cols.arg] = cmd->cmd;
			RSendKey key = boost::dynamic_pointer_cast<SendKey>(si.action);
			if (key) {
				row[cols.arg] = key->get_label();
				row[cols.type] = key->xtest ? KEY_XTEST : KEY;
			}
			RScroll scroll = boost::dynamic_pointer_cast<Scroll>(si.action);
			if (scroll) {
				row[cols.arg] = scroll->get_label();
				row[cols.type] = SCROLL;
			}
			RIgnore ignore = boost::dynamic_pointer_cast<Ignore>(si.action);
			if (ignore) {
				row[cols.arg] = ignore->get_label();
				row[cols.type] = IGNORE;
			}
			RButton button = boost::dynamic_pointer_cast<Button>(si.action);
			if (button) {
				row[cols.arg] = button->get_label();
				row[cols.type] = BUTTON;
			}
		}
	}

	int n;
	tv->append_column("Stroke", cols.stroke);

	n = tv->append_column("Name", cols.name);
	Gtk::CellRendererText *name = dynamic_cast<Gtk::CellRendererText *>(tv->get_column_cell_renderer(n-1));
	name->property_editable() = true;
	name->signal_edited().connect(sigc::mem_fun(*this, &Actions::on_name_edited));
	name->signal_editing_started().connect(sigc::mem_fun(*this, &Actions::on_something_editing_started));
	name->signal_editing_canceled().connect(sigc::mem_fun(*this, &Actions::on_something_editing_canceled));
	Gtk::TreeView::Column *col_name = tv->get_column(n-1);
	col_name->set_sort_column(cols.name);

	type_store = Gtk::ListStore::create(type);
	(*(type_store->append()))[type.type] = COMMAND;
	if (experimental)
		(*(type_store->append()))[type.type] = KEY;
	(*(type_store->append()))[type.type] = KEY_XTEST;
	(*(type_store->append()))[type.type] = IGNORE;
	(*(type_store->append()))[type.type] = SCROLL;
	(*(type_store->append()))[type.type] = BUTTON;
	type_renderer.property_model() = type_store;
	type_renderer.property_editable() = true;
	type_renderer.property_text_column() = 0;
	type_renderer.property_has_entry() = false;
	type_renderer.signal_edited().connect(sigc::mem_fun(*this, &Actions::on_type_edited));
	type_renderer.signal_editing_started().connect(sigc::mem_fun(*this, &Actions::on_something_editing_started));
	type_renderer.signal_editing_canceled().connect(sigc::mem_fun(*this, &Actions::on_something_editing_canceled));

	n = tv->append_column("Type", type_renderer);
	Gtk::TreeView::Column *col_type = tv->get_column(n-1);
	col_type->add_attribute(type_renderer.property_text(), cols.type);

	n = tv->append_column("Argument", accel_renderer);
	Gtk::TreeView::Column *col_accel = tv->get_column(n-1);
	col_accel->add_attribute(accel_renderer.property_text(), cols.arg);
	accel_renderer.property_editable() = true;
	accel_renderer.signal_accel_edited().connect(sigc::mem_fun(*this, &Actions::on_accel_edited));
	accel_renderer.signal_edited().connect(sigc::mem_fun(*this, &Actions::on_cmd_edited));
	accel_renderer.signal_editing_started().connect(sigc::mem_fun(*this, &Actions::on_arg_editing_started));

	tv->set_model(tm);
}

void Actions::write() {
	if (!good_state)
		return;
	Setter s;
	good_state = s.ref(actions).write();
	if (!good_state) {
		Gtk::MessageDialog dialog(win->get_window(), "Couldn't save actions.  Your changes will be lost.  \nMake sure that "+config_dir+" is a directory and that you have write access to it.\nYou can change the configuration directory using the -c or --config-dir command line options.", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.run();
	}
}

void Actions::on_type_edited(const Glib::ustring& path, const Glib::ustring& new_type) {
	tv->grab_focus();
	Gtk::TreeRow row(*tm->get_iter(path));
	Glib::ustring old_type = row[cols.type];
	bool edit = true;
	if (old_type == new_type) {
		edit = editing_new;
	} else {
#define IS_KEY(str) (str == KEY || str == KEY_XTEST)
		bool both_keys = IS_KEY(new_type) && IS_KEY(old_type);
		row[cols.type] = new_type;
		int id = row[cols.id];
		Setter s;
		ActionDB &as = s.write_ref(actions);
		if (both_keys) {
			RSendKey key = boost::dynamic_pointer_cast<SendKey>(as[id].action);
			if (key) {
				key->xtest = row[cols.type] == KEY_XTEST;
				write();
			}
			edit = false;
		} else {
			if (new_type == COMMAND) {
				Glib::ustring cmd_save = row[cols.cmd_save];
				row[cols.arg] = cmd_save;
				if (cmd_save != "")
					edit = false;

				as[id].action.reset();
				write();
			}
			if (old_type == COMMAND) {
				row[cols.cmd_save] = (Glib::ustring)row[cols.arg];
				update_arg(new_type);
			}
			if (new_type == SCROLL) {
				row[cols.arg] = "No Modifiers";
				update_arg(new_type);
				as[id].action = Scroll::create((Gdk::ModifierType)0);
				write();
				edit = false;
			}
			if (new_type == IGNORE) {
				row[cols.arg] = "No Modifiers";
				update_arg(new_type);
				as[id].action = Ignore::create((Gdk::ModifierType)0);
				write();
				edit = false;
			}
			if (new_type == BUTTON) {
				row[cols.arg] = "";
				update_arg(new_type);
				as[id].action = Button::create((Gdk::ModifierType)0, 0);
				write();
				edit = true;
			}
		}
		row[cols.type] = new_type;
	}
	editing_new = false;
	focus(row[cols.id], 3, edit);
}

void Actions::on_button_delete() {
	int n = tv->get_selection()->count_selected_rows();

	std::stringstream msg;
	if (n == 1)
		msg << "Action \"" << get_selected_row()[cols.name] << "\" is";
	else
		msg << n << " actions are";

	Gtk::Dialog *dialog;
	widgets->get_widget("dialog_delete", dialog);
	FormatLabel foo(widgets, "label_delete", msg.str().c_str());

	bool ok = dialog->run() == 1;
	dialog->hide();
	if (!ok)
		return;

	// complete craziness
	std::vector<Gtk::TreePath> paths = tv->get_selection()->get_selected_rows();
	std::vector<Gtk::TreeRowReference> refs;
	std::vector<int> ids;
	for (std::vector<Gtk::TreePath>::iterator i = paths.begin(); i != paths.end(); ++i) {
		Gtk::TreeRowReference ref(tm, *i);
		refs.push_back(ref);
		Gtk::TreeRow row(*tm->get_iter(*i));
		int id = row[cols.id];
		ids.push_back(id);
	}
	for (std::vector<Gtk::TreeRowReference>::iterator i = refs.begin(); i != refs.end(); ++i)
		tm->erase(*tm->get_iter(i->get_path()));
	Setter s;
	ActionDB &as = s.write_ref(actions);
	for (std::vector<int>::iterator i = ids.begin(); i != ids.end(); ++i)
		as.remove(*i);
	write();
}

class Actions::OnStroke {
	Actions *parent;
	Gtk::Dialog *dialog;
	int id;
	Gtk::TreeValueProxy<Glib::RefPtr<Gdk::Pixbuf> > pb;
public:
	OnStroke(Actions *parent_, Gtk::Dialog *dialog_, int id_, Gtk::TreeValueProxy<Glib::RefPtr<Gdk::Pixbuf> > pb_)
		: parent(parent_), dialog(dialog_), id(id_), pb(pb_) {}
	void run(RStroke stroke) {
		Setter s;
		ActionDB &as = s.write_ref(actions);
		StrokeInfo &si = as[id];
		si.strokes.clear();
		si.strokes.insert(stroke);
		parent->write();
		dialog->response(0);
		Glib::RefPtr<Gdk::Pixbuf> pb2 = stroke->draw(STROKE_SIZE);
		pb = pb2;
	}
};

void Actions::on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column) {
	Gtk::Dialog *dialog;
	widgets->get_widget("dialog_record", dialog);
	Gtk::TreeRow row(*tm->get_iter(path));
	FormatLabel foo(widgets, "label_record", Glib::ustring(row[cols.name]).c_str());

	Gtk::Button *del;
	widgets->get_widget("button_delete_current", del);
	// TODO: What if find fails?
	Setter s;
	del->set_sensitive(s.ref(actions).lookup(row[cols.id]).strokes.size());

	OnStroke ps(this, dialog, row[cols.id], row[cols.stroke]);
	stroke_action.set2(sigc::mem_fun(ps, &OnStroke::run));

	int response = dialog->run();
	dialog->hide();
	stroke_action.erase();
	if (response != 1)
		return;

	row[cols.stroke] = Stroke::drawEmpty(STROKE_SIZE);
	s.write_ref(actions)[row[cols.id]].strokes.clear();
	write();
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
	update_arg(row[cols.type]);
}

Gtk::TreeRow Actions::get_selected_row() {
	std::vector<Gtk::TreePath> paths = tv->get_selection()->get_selected_rows();
	return Gtk::TreeRow(*tm->get_iter(*paths.begin()));
}

void Actions::on_selection_changed() {
	int n = tv->get_selection()->count_selected_rows();
	button_record->set_sensitive(n == 1);
	button_delete->set_sensitive(n >= 1);
}

void Actions::update_arg(Glib::ustring str) {
	GtkCellRendererTKMode mode;
	if (IS_KEY(str) || str == SCROLL || str == IGNORE)
		mode = 	GTK_CELL_RENDERER_TK_CELL_MODE_KEY;
	else if (str == BUTTON)
		mode = GTK_CELL_RENDERER_TK_CELL_MODE_POPUP;
	else
		mode = GTK_CELL_RENDERER_TK_CELL_MODE_TEXT;
	// 	Fuck it, I'm not going to figure out how to make GtkCellRendererTKMode a proper GObject.
	//	accel_renderer.property_cell_mode() = mode
	g_object_set(G_OBJECT(accel_renderer.gobj()), "cell-mode", mode, NULL);
}

void Actions::on_button_new() {
	editing_new = true;

	Gtk::TreeModel::Row row = *(tm->append());
	row[cols.stroke] = Stroke::drawEmpty(STROKE_SIZE);
	row[cols.type] = COMMAND;
	char buf[16];
	Setter s;
	snprintf(buf, 15, "Gesture %d", s.ref(actions).size()+1);
	row[cols.id] = s.write_ref(actions).addCmd(RStroke(), buf, "");
	row[cols.name] = buf;

	Gtk::TreePath path = tm->get_path(row);
	focus(row[cols.id], 1, true);
}

struct Actions::Focus {
	Actions *parent;
	int id;
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

void Actions::focus(int id, int col, bool edit) {
	// More C++ closure fun.
	Focus* focus = new Focus;
	focus->parent = this;
	focus->id = id;
	focus->col = tv->get_column(col);
	focus->edit = edit;
	editing = false;
	Glib::signal_timeout().connect(sigc::mem_fun(*focus, &Focus::focus), 0, Glib::PRIORITY_LOW);
}

void Actions::on_name_edited(const Glib::ustring& path, const Glib::ustring& new_text) {
	Gtk::TreeRow row(*tm->get_iter(path));
	Setter s;
	s.write_ref(actions)[row[cols.id]].name = new_text;
	write();
	row[cols.name] = new_text;
	focus(row[cols.id], 2, editing_new);
}

void Actions::on_cmd_edited(const Glib::ustring& path, const Glib::ustring& new_cmd) {
	Gtk::TreeRow row(*tm->get_iter(path));
	row[cols.arg] = new_cmd;
	int id = row[cols.id];
	Setter s;
	ActionDB &as = s.write_ref(actions);
	RCommand c = boost::dynamic_pointer_cast<Command>(as[id].action);
	if (c)
		c->cmd = new_cmd;
	else
		as[id].action = Command::create(new_cmd);
	write();
}

void Actions::on_accel_edited(const Glib::ustring& path_string, guint accel_key, Gdk::ModifierType accel_mods, guint hardware_keycode) {
	Gtk::TreeRow row(*tm->get_iter(path_string));
	Setter s;
	ActionDB &as = s.write_ref(actions);
	if (IS_KEY(row[cols.type])) {
		RSendKey send_key = SendKey::create(accel_key, accel_mods, hardware_keycode, row[cols.type] == KEY_XTEST);
		Glib::ustring str = send_key->get_label();
		if (row[cols.arg] == str)
			return;
		row[cols.arg] = str;
		as[row[cols.id]].action = boost::static_pointer_cast<Action>(send_key);
		write();
	}
	if (row[cols.type] == SCROLL) {
		RScroll scroll = Scroll::create(accel_mods);
		Glib::ustring str = scroll->get_label();
		if (row[cols.arg] == str)
			return;
		row[cols.arg] = str;
		as[row[cols.id]].action = boost::static_pointer_cast<Action>(scroll);
		write();
	}
	if (row[cols.type] == IGNORE) {
		RIgnore ignore = Ignore::create(accel_mods);
		Glib::ustring str = ignore->get_label();
		if (row[cols.arg] == str)
			return;
		row[cols.arg] = str;
		as[row[cols.id]].action = boost::static_pointer_cast<Action>(ignore);
		write();
	}
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
	Setter s;
	ActionDB &as = s.write_ref(actions);
	RButton bt = boost::static_pointer_cast<Button>(as[row[cols.id]].action);
	ButtonInfo bi = bt->get_button_info();
	SelectButton sb(bi, false);
	if (!sb.run())
		return;
	bt = boost::static_pointer_cast<Button>(Button::create(Gdk::ModifierType(sb.event.state), sb.event.button));
	as[row[cols.id]].action = bt;
	row[cols.arg] = bt->get_label();
	write();

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
