#include "actions.h"
#include "actiondb.h"
#include "win.h"
#include "strokeaction.h"
#include "main.h"

#include <iostream>
#include <sstream>

#define KEY "Key (SendKey)"
#define KEY_XTEST "Key"
#define COMMAND "Command"
#define SCROLL "Scroll"
#define IGNORE "Ignore"

class ActionDBRef : public Ref<ActionDB> {
public:
	ActionDBRef(LActionDB& db) : Ref<ActionDB>(db) {}
};

Actions::Actions(Win *p) :
	good_state(true),
	parent(p),
	tv(0),
	editing_new(false),
	editing(false)
{
	parent->widgets->get_widget("treeview_actions", tv);

	Gtk::Button *button_add = 0;
	parent->widgets->get_widget("button_add_action", button_add);
	parent->widgets->get_widget("button_delete_action", button_delete);
	parent->widgets->get_widget("button_record", button_record);
	button_record->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_record));
	button_delete->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_delete));
	button_add->signal_clicked().connect(sigc::mem_fun(*this, &Actions::on_button_new));

	tv->signal_cursor_changed().connect(sigc::mem_fun(*this, &Actions::on_cursor_changed));
	tv->signal_row_activated().connect(sigc::mem_fun(*this, &Actions::on_row_activated));
	tv->get_selection()->signal_changed().connect(sigc::mem_fun(*this, &Actions::on_selection_changed));

	tm = Gtk::ListStore::create(cols);

	tv->get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);

	Gtk::TreeModel::Row row;
	{
		ActionDBRef ref(actions());
		for (ActionDB::const_iterator i = ref->begin(); i!=ref->end(); i++) {
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
		}
	}

	int n;
	tv->append_column("Stroke", cols.stroke);

	n = tv->append_column("Name", cols.name);
	Gtk::CellRendererText *name = dynamic_cast<Gtk::CellRendererText *>(tv->get_column_cell_renderer(n-1));
	name->property_editable() = true;
	name->signal_edited().connect(sigc::mem_fun(*this, &Actions::on_name_edited));
	name->signal_editing_started().connect(sigc::mem_fun(*this, &Actions::on_something_editing_started));
	Gtk::TreeView::Column *col_name = tv->get_column(n-1);
	col_name->set_sort_column(cols.name);

	type_store = Gtk::ListStore::create(type);
	(*(type_store->append()))[type.type] = COMMAND;
//	(*(type_store->append()))[type.type] = KEY;
	(*(type_store->append()))[type.type] = KEY_XTEST;
	(*(type_store->append()))[type.type] = IGNORE;
	(*(type_store->append()))[type.type] = SCROLL;
	type_renderer.property_model() = type_store;
	type_renderer.property_editable() = true;
	type_renderer.property_text_column() = 0;
	type_renderer.property_has_entry() = false;
	type_renderer.signal_edited().connect(sigc::mem_fun(*this, &Actions::on_type_edited));
	type_renderer.signal_editing_started().connect(sigc::mem_fun(*this, &Actions::on_something_editing_started));

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

void Actions::write(ActionDBRef& ref) {
	if (!good_state)
		return;
	good_state = ref->write();
	if (!good_state) {
		Gtk::MessageDialog dialog(parent->get_window(), "Couldn't save actions.  Your changes will be lost.  \nMake sure that "+config_dir+" is a directory and that you have write access to it.\nYou can change the configuration directory using the -c or --config-dir command line options.", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.run();
	}
}

void Actions::on_type_edited(const Glib::ustring& path, const Glib::ustring& new_type) {
	editing = false;
	tv->grab_focus();
	Gtk::TreeRow row(*tm->get_iter(path));
	Glib::ustring old_type = row[cols.type];
	if (old_type == new_type)
		return;
#define IS_KEY(str) (str == KEY || str == KEY_XTEST)
	bool both_keys = IS_KEY(new_type) && IS_KEY(old_type);
	row[cols.type] = new_type;
	int id = row[cols.id];
	bool edit = true;
	if (both_keys) {
		ActionDBRef ref(actions());
		RSendKey key = boost::dynamic_pointer_cast<SendKey>((*ref)[id].action);
		if (key) {
			key->xtest = row[cols.type] == KEY_XTEST;
			write(ref);
		}
		edit = false;
	} else {
		if (new_type == COMMAND) {
			Glib::ustring cmd_save = row[cols.cmd_save];
			row[cols.arg] = cmd_save;
			if (cmd_save != "")
				edit = false;

			ActionDBRef ref(actions());
			(*ref)[id].action.reset();
			write(ref);
		}
		if (old_type == COMMAND) {
			row[cols.cmd_save] = (Glib::ustring)row[cols.arg];
			update_arg(new_type);
		}
		if (new_type == SCROLL) {
			row[cols.arg] = "No Modifiers";
			update_arg(new_type);
			ActionDBRef ref(actions());
			(*ref)[id].action = Scroll::create((Gdk::ModifierType)0);
			write(ref);
			edit = false;
		}
		if (new_type == IGNORE) {
			row[cols.arg] = "No Modifiers";
			update_arg(new_type);
			ActionDBRef ref(actions());
			(*ref)[id].action = Ignore::create((Gdk::ModifierType)0);
			write(ref);
			edit = false;
		}
	}
	row[cols.type] = new_type;
	tv->set_cursor(Gtk::TreePath(path), *tv->get_column(3), edit);
}

void Actions::on_button_delete() {
	if (1) {
		int n = tv->get_selection()->count_selected_rows();
		std::stringstream msg;
		if (n == 1)
			msg << "Action \"" << get_selected_row()[cols.name] << "\" is";
		else
			msg << n << " actions are";
		msg << " about to be deleted.";
		Gtk::MessageDialog dialog(parent->get_window(), msg.str(), false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK_CANCEL, true);
	if (dialog.run() != Gtk::RESPONSE_OK)
		return;
	} else {
		if (run_dialog("dialog_delete") != 1)
			return;
	}
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
	ActionDBRef ref(actions());
	for (std::vector<int>::iterator i = ids.begin(); i != ids.end(); ++i)
		ref->remove(*i);
	write(ref);
}

class Actions::OnStroke {
	Actions *parent;
	Gtk::Dialog *dialog;
	int id;
	Gtk::TreeValueProxy<Glib::RefPtr<Gdk::Pixbuf> > pb;
public:
	OnStroke(Actions *parent_, Gtk::Dialog *dialog_, int id_, Gtk::TreeValueProxy<Glib::RefPtr<Gdk::Pixbuf> > pb_)
		: parent(parent_), dialog(dialog_), id(id_), pb(pb_) {}
	void run(RStroke s) {
		ActionDBRef ref(actions());
		StrokeInfo &si = (*ref)[id];
		si.strokes.clear();
		si.strokes.insert(s);
		parent->write(ref);
		dialog->hide();
		Glib::RefPtr<Gdk::Pixbuf> pb2 = s->draw(STROKE_SIZE);
		pb = pb2;
	}
};

void Actions::on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column) {
	Gtk::Dialog dialog("Record Stroke", parent->get_window(), true);
	Gtk::HBox hbox(false,12);
	dialog.get_vbox()->set_spacing(30);
	dialog.get_vbox()->pack_start(hbox);
	Gtk::Image image(Gtk::Stock::DIALOG_INFO, Gtk::ICON_SIZE_DIALOG);
	hbox.pack_start(image);
	Gtk::TreeRow row(*tm->get_iter(path));
	Gtk::Label label("The next stroke will be associated with the action \"" + row[cols.name] + "\".");
	label.set_line_wrap();
	label.set_size_request(200,-1);
	hbox.pack_start(label);
	dialog.add_button("_Delete Current",0);
	dialog.add_button(Gtk::Stock::CANCEL,1);
	dialog.show_all_children();

	OnStroke ps(this, &dialog, row[cols.id], row[cols.stroke]);
	stroke_action().set(sigc::mem_fun(ps, &OnStroke::run));

	int response = dialog.run();
	stroke_action().erase();
	if (response != 0)
		return;

	row[cols.stroke] = Stroke::drawEmpty(STROKE_SIZE);
	ActionDBRef ref(actions());
	(*ref)[row[cols.id]].strokes.clear();
	write(ref);
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
	GtkCellRendererTKMode mode = (IS_KEY(str) || str == SCROLL || str == IGNORE) ?
		GTK_CELL_RENDERER_TK_CELL_MODE_KEY :
		GTK_CELL_RENDERER_TK_CELL_MODE_TEXT;
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
	snprintf(buf, 15, "Gesture %d", actions().get().size()+1);
	row[cols.id] = actions().addCmd(RStroke(), buf, "");
	row[cols.name] = buf;

	Gtk::TreePath path = tm->get_path(row);
	focus(path.to_string(), 1, true);
}

struct Actions::Focus {
	Actions *parent;
	Gtk::TreePath path;
	Gtk::TreeViewColumn* col;
	bool edit;
	bool focus() {
		if (!parent->editing)
			parent->tv->set_cursor(path, *col, edit);
		delete this;
		return false;
	}
};

void Actions::focus(const Glib::ustring& path, int col, bool edit) {
	// More C++ closure fun.
	Focus* focus = new Focus;
	focus->parent = this;
	focus->path = Gtk::TreePath(path);
	focus->col = tv->get_column(col);
	focus->edit = edit;
	Glib::signal_timeout().connect(sigc::mem_fun(*focus, &Focus::focus), 0, Glib::PRIORITY_LOW);
}

void Actions::on_name_edited(const Glib::ustring& path, const Glib::ustring& new_text) {
	editing = false;
	Gtk::TreeRow row(*tm->get_iter(path));
	row[cols.name] = new_text;
	{
		ActionDBRef ref(actions());
		(*ref)[row[cols.id]].name = new_text;
		write(ref);
	}
	focus(path, 2, editing_new);
	editing_new = false;
}

void Actions::on_cmd_edited(const Glib::ustring& path, const Glib::ustring& new_cmd) {
	editing = false;
	Gtk::TreeRow row(*tm->get_iter(path));
	ActionDBRef ref(actions());
	row[cols.arg] = new_cmd;
	int id = row[cols.id];
	RCommand c = boost::dynamic_pointer_cast<Command>((*ref)[id].action);
	if (c)
		c->cmd = new_cmd;
	else
		(*ref)[id].action = Command::create(new_cmd);
	write(ref);
}

void Actions::on_accel_edited(const Glib::ustring& path_string, guint accel_key, Gdk::ModifierType accel_mods, guint hardware_keycode) {
	editing = false;
	Gtk::TreeRow row(*tm->get_iter(path_string));
	if (IS_KEY(row[cols.type])) {
		RSendKey send_key = SendKey::create(accel_key, accel_mods, hardware_keycode, row[cols.type] == KEY_XTEST);
		Glib::ustring str = send_key->get_label();
		if (row[cols.arg] == str)
			return;
		row[cols.arg] = str;
		ActionDBRef ref(actions());
		(*ref)[row[cols.id]].action = boost::static_pointer_cast<Action>(send_key);
		write(ref);
	}
	if (row[cols.type] == SCROLL) {
		RScroll scroll = Scroll::create(accel_mods);
		Glib::ustring str = scroll->get_label();
		if (row[cols.arg] == str)
			return;
		row[cols.arg] = str;
		ActionDBRef ref(actions());
		(*ref)[row[cols.id]].action = boost::static_pointer_cast<Action>(scroll);
		write(ref);
	}
	if (row[cols.type] == IGNORE) {
		RIgnore ignore = Ignore::create(accel_mods);
		Glib::ustring str = ignore->get_label();
		if (row[cols.arg] == str)
			return;
		row[cols.arg] = str;
		ActionDBRef ref(actions());
		(*ref)[row[cols.id]].action = boost::static_pointer_cast<Action>(ignore);
		write(ref);
	}
}

void Actions::on_something_editing_started(Gtk::CellEditable* editable, const Glib::ustring& path) {
	editing = true;
}

void Actions::on_arg_editing_started(Gtk::CellEditable* editable, const Glib::ustring& path) {
	tv->grab_focus();
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
