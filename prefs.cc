#include "prefs.h"
#include "win.h"
#include "main.h"

#include <X11/Xutil.h>
extern "C" {
#include "dsimple.h"
void usage() {}
}

#include <set>
#include <iostream>

Prefs::Prefs(Win *parent_) :
	good_state(true), parent(parent_), q(sigc::mem_fun(*this, &Prefs::on_selected))
{
	Gtk::Button *bbutton, *add_exception, *remove_exception, *button_default_p, *button_default_radius;
	parent->widgets->get_widget("button_add_exception", add_exception);
	parent->widgets->get_widget("button_button", bbutton);
	parent->widgets->get_widget("button_default_p", button_default_p);
	parent->widgets->get_widget("button_remove_exception", remove_exception);
	parent->widgets->get_widget("combo_trace", trace);
	parent->widgets->get_widget("label_button", blabel);
	parent->widgets->get_widget("treeview_exceptions", tv);
	parent->widgets->get_widget("scale_p", scale_p);
	parent->widgets->get_widget("check_advanced_ignore", advanced_ignore);
	parent->widgets->get_widget("spin_radius", spin_radius);
	parent->widgets->get_widget("button_default_radius", button_default_radius);

	tm = Gtk::ListStore::create(cols);
	tv->set_model(tm);
	tv->append_column("WM__CLASS name", cols.col);
	tm->set_sort_column(cols.col, Gtk::SORT_ASCENDING);

	bbutton->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_select_button));

	add_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_add));
	remove_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_remove));

	trace->signal_changed().connect(sigc::mem_fun(*this, &Prefs::on_trace_changed));
	trace->set_active(prefs().trace.get());

	advanced_ignore->signal_toggled().connect(sigc::mem_fun(*this, &Prefs::on_advanced_ignore_changed));
	advanced_ignore->set_active(prefs().advanced_ignore.get());

	double p = prefs().p.get();
	scale_p->set_value(p);
	scale_p->signal_value_changed().connect(sigc::mem_fun(*this, &Prefs::on_p_changed));
	button_default_p->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_p_default));

	spin_radius->set_value(prefs().radius.get());
	spin_radius->signal_value_changed().connect(sigc::mem_fun(*this, &Prefs::on_radius_changed));
	button_default_radius->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_radius_default));

	if (!experimental) {
		Gtk::HBox *hbox_experimental;
	       	parent->widgets->get_widget("hbox_experimental", hbox_experimental);
		hbox_experimental->hide();
	}
	set_button_label();

	RPrefEx exceptions(prefs().exceptions);
	for (std::set<std::string>::iterator i = exceptions->begin(); i!=exceptions->end(); i++) {
		Gtk::TreeModel::Row row = *(tm->append());
		row[cols.col] = *i;
	}
}

struct Prefs::SelectRow {
	Prefs *parent;
	std::string name;
	bool test(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter) {
		if ((*iter)[parent->cols.col] == name) {
			parent->tv->set_cursor(path);
			return true;
		}
		return false;
	}
};

void Prefs::write() {
	if (!good_state)
		return;
	good_state = prefs().write();
	if (!good_state) {
		Gtk::MessageDialog dialog(parent->get_window(), "Couldn't save preferences.  Your changes will be lost.  \nMake sure that "+config_dir+" is a directory and that you have write access to it.\nYou can change the configuration directory using the -c or --config-dir command line options.", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.run();
	}
}

SelectButton::SelectButton(const Glib::RefPtr<Gtk::Builder> widgets, ButtonInfo bi, bool def) {
	widgets->get_widget("dialog_select", dialog);
	widgets->get_widget("eventbox", eventbox);
	widgets->get_widget("toggle_shift", toggle_shift);
	widgets->get_widget("toggle_alt", toggle_alt);
	widgets->get_widget("toggle_control", toggle_control);
	widgets->get_widget("toggle_super", toggle_super);
	widgets->get_widget("select_button", select_button);
	select_button->set_active(bi.button-1);
	toggle_shift->set_active(bi.button && (bi.state & GDK_SHIFT_MASK));
	toggle_control->set_active(bi.button && (bi.state & GDK_CONTROL_MASK));
	toggle_alt->set_active(bi.button && (bi.state & GDK_MOD1_MASK));
	toggle_super->set_active(bi.button && (bi.state & GDK_MOD4_MASK));

	Gtk::Button *select_default;
	widgets->get_widget("select_default", select_default);
	select_default->set_sensitive(def);

	if (!eventbox->get_children().size()) {
		eventbox->set_events(Gdk::BUTTON_PRESS_MASK);

		Glib::RefPtr<Gdk::Pixbuf> pb = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB,true,8,400,200);
		pb->fill(0x808080ff);
		WIDGET(Gtk::Image, box, pb);
		eventbox->add(box);
		box.show();
	}
	handler = eventbox->signal_button_press_event().connect(sigc::mem_fun(*this, &SelectButton::on_button_press));
}

SelectButton::~SelectButton() {
	handler.disconnect();
}

bool SelectButton::run() {
	send(P_SUSPEND_GRAB);
	int response;
	do {
		response = dialog->run();
	} while (response == 0);
	dialog->hide();
	send(P_RESTORE_GRAB);
	switch (response) {
		case 1: // Okay
			event.button = select_button->get_active_row_number() + 1;
			if (!event.button)
				return false;
			event.state = 0;
			if (toggle_shift->get_active())
				event.state |= GDK_SHIFT_MASK;
			if (toggle_control->get_active())
				event.state |= GDK_CONTROL_MASK;
			if (toggle_alt->get_active())
				event.state |= GDK_MOD1_MASK;
			if (toggle_super->get_active())
				event.state |= GDK_MOD4_MASK;
			return true;
		case 2: // Default
			event.button = prefs().button.get().button;
			event.state = 0;
			return true;
		case 3: // Click - all the work has already been done
			return true;
		case -1: // Cancel
		default: // Something went wrong
			return false;
	}
}

bool SelectButton::on_button_press(GdkEventButton *ev) {
	event = *ev;
	dialog->response(3);
	return true;
}

void Prefs::on_select_button() {
	SelectButton sb(parent->widgets, prefs().button.get());
	if (!sb.run())
		return;
	{
		Ref<ButtonInfo> ref(prefs().button);
		ref->button = sb.event.button;
		ref->state = sb.event.state;
	}
	send(P_REGRAB);
	set_button_label();
	write();
}

void Prefs::on_trace_changed() {
	TraceType type = (TraceType) trace->get_active_row_number();
	if (type >= trace_n)
		return;
	if (prefs().trace.get() == type)
		return;
	prefs().trace.set(type);
	send(P_UPDATE_TRACE);
	write();
}

void Prefs::on_advanced_ignore_changed() {
	prefs().advanced_ignore.set(advanced_ignore->get_active());
	write();
}

void Prefs::on_p_changed() {
	prefs().p.set(scale_p->get_value());
	write();
}

void Prefs::on_p_default() {
	scale_p->set_value(pref_p_default);
}

void Prefs::on_radius_changed() {
	prefs().radius.set(spin_radius->get_value());
	write();
}

void Prefs::on_radius_default() {
	spin_radius->set_value(pref_radius_default);
}

void Prefs::on_selected(std::string &str) {
	if (RPrefEx(prefs().exceptions)->insert(str).second) {
		Gtk::TreeModel::Row row = *(tm->append());
		row[cols.col] = str;
		Gtk::TreePath path = tm->get_path(row);
		tv->set_cursor(path);
		send(P_UPDATE_CURRENT);
	} else {
		SelectRow cb;
		cb.name = str;
		cb.parent = this;
		tm->foreach(sigc::mem_fun(cb, &SelectRow::test));
	}
}

void Prefs::on_remove() {
	Gtk::TreePath path;
	Gtk::TreeViewColumn *col;
	tv->get_cursor(path, col);
	if (path.gobj() != 0) {
		Gtk::TreeIter iter = *tm->get_iter(path);
		RPrefEx(prefs().exceptions)->erase((Glib::ustring)((*iter)[cols.col]));
		tm->erase(iter);
		send(P_UPDATE_CURRENT);
	}
}

void Prefs::on_add() {
	Glib::Thread::create(sigc::mem_fun(*this, &Prefs::select_worker), false);
}

void Prefs::select_worker() {
	Display *dpy = XOpenDisplay(NULL);
	if (!dpy)
		return;
	Window w = Select_Window(dpy);
	XClassHint ch;
	if (!XGetClassHint(dpy, w, &ch))
		goto cleanup;
	{
		std::string str(ch.res_name);
		q.push(str);
	}
	XFree(ch.res_name);
	XFree(ch.res_class);
cleanup:
	XCloseDisplay(dpy);
}


void Prefs::set_button_label() {
	Ref<ButtonInfo> ref(prefs().button);
	blabel->set_text(ref->get_button_text());
}
