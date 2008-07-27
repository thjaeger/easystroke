#include "prefs.h"
#include "win.h"
#include "main.h"
#include "grabber.h"

#include <X11/Xutil.h>
extern "C" {
#include "dsimple.h"
}

#include <set>
#include <iostream>

bool good_state = true;

void write_prefs() {
	if (!good_state)
		return;
	good_state = prefs.write();
	if (!good_state) {
		Gtk::MessageDialog dialog(win->get_window(), "Couldn't save preferences.  Your changes will be lost.  \nMake sure that "+config_dir+" is a directory and that you have write access to it.\nYou can change the configuration directory using the -c or --config-dir command line options.", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dialog.run();
	}
}

Check::Check(const Glib::ustring &name, VarI<bool> &b_) : b(b_) {
	widgets->get_widget(name, check);
	check->signal_toggled().connect(sigc::mem_fun(*this, &Check::on_changed));
	check->set_active(b.get());
}

void Check::on_changed() {
	b.set(check->get_active());
	write_prefs();
}

Pressure::Pressure() :
	Check("check_pressure_abort", prefs.pressure_abort),
	spin("spin_pressure_threshold", "button_default_pressure_threshold",
			prefs.pressure_threshold, default_pressure_threshold)
{}

void Pressure::on_changed() {
	Check::on_changed();
	spin.spin->set_sensitive(b.get());
	spin.button->set_sensitive(b.get());
}

Spin::Spin(const Glib::ustring &name, const Glib::ustring &default_name, VarI<int> &i_, const int def_) : i(i_), def(def_) {
	widgets->get_widget(name, spin);
	widgets->get_widget(default_name, button);
	spin->set_value(i.get());
	spin->signal_value_changed().connect(sigc::mem_fun(*this, &Spin::on_changed));
	button->signal_clicked().connect(sigc::mem_fun(*this, &Spin::on_default));
}

void Spin::on_changed() {
	i.set(spin->get_value());
	write_prefs();
}

void Spin::on_default() {
	spin->set_value(def);
}

Proximity::Proximity() : Check("check_proximity", prefs.proximity) {}

void Proximity::on_changed() {
	Check::on_changed();
	send(P_PROXIMITY);
}

extern Glib::Mutex *grabber_mutex; //TODO: This is a hack

Prefs::Prefs() :
	q(sigc::mem_fun(*this, &Prefs::on_selected)),
	advanced_ignore("check_advanced_ignore", prefs.advanced_ignore),
	ignore_grab("check_ignore_grab", prefs.ignore_grab),
	timing_workaround("check_timing_workaround", prefs.timing_workaround),
	show_clicks("check_show_clicks", prefs.show_clicks),
	radius("spin_radius", "button_default_radius", prefs.radius, default_radius)
{
	Gtk::Button *bbutton, *add_exception, *remove_exception, *button_default_p;
	widgets->get_widget("button_add_exception", add_exception);
	widgets->get_widget("button_button", bbutton);
	widgets->get_widget("button_default_p", button_default_p);
	widgets->get_widget("button_remove_exception", remove_exception);
	widgets->get_widget("combo_trace", trace);
	widgets->get_widget("label_button", blabel);
	widgets->get_widget("treeview_exceptions", tv);
	widgets->get_widget("scale_p", scale_p);

	grabber_mutex->lock();
	grabber_mutex->unlock();
	delete grabber_mutex;
	grabber_mutex = 0;

	Gtk::Widget *widget;
	widgets->get_widget("check_timing_workaround", widget);
	widget->set_sensitive(grabber->xinput);
	widgets->get_widget("check_ignore_grab", widget);
	widget->set_sensitive(grabber->xinput);
	widgets->get_widget("hbox_pressure", widget);
	widget->set_sensitive(grabber->supports_pressure());
	widgets->get_widget("check_proximity", widget);
	widget->set_sensitive(grabber->supports_proximity());

	tm = Gtk::ListStore::create(cols);
	tv->set_model(tm);
	tv->append_column("WM__CLASS name", cols.col);
	tm->set_sort_column(cols.col, Gtk::SORT_ASCENDING);

	bbutton->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_select_button));

	add_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_add));
	remove_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_remove));

	trace->signal_changed().connect(sigc::mem_fun(*this, &Prefs::on_trace_changed));
	trace->set_active(prefs.trace.get());

	double p = prefs.p.get();
	scale_p->set_value(p);
	scale_p->signal_value_changed().connect(sigc::mem_fun(*this, &Prefs::on_p_changed));
	button_default_p->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_p_default));

	if (!experimental) {
		Gtk::HBox *hbox_experimental;
	       	widgets->get_widget("hbox_experimental", hbox_experimental);
		hbox_experimental->hide();
	}
	set_button_label();

	Setter s;
	std::set<std::string> exceptions = s.ref(prefs.exceptions);
	for (std::set<std::string>::iterator i = exceptions.begin(); i!=exceptions.end(); i++) {
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

SelectButton::SelectButton(ButtonInfo bi, bool def) {
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
	toggle_super->set_active(bi.button && (bi.state & GDK_SUPER_MASK));

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
				event.state |= GDK_SUPER_MASK;
			return true;
		case 2: // Default
			event.button = prefs.button.get().button;
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
	if (event.state & Mod4Mask)
		event.state |= GDK_SUPER_MASK;
	event.state &= gtk_accelerator_get_default_mod_mask();
	dialog->response(3);
	return true;
}

void Prefs::on_select_button() {
	Setter s;
	ButtonInfo &bi = s.write_ref(prefs.button);
	SelectButton sb(bi);
	if (!sb.run())
		return;
	{
		bi.button = sb.event.button;
		bi.state = sb.event.state;
	}
	send(P_REGRAB);
	set_button_label();
	write_prefs();
}

void Prefs::on_trace_changed() {
	TraceType type = (TraceType) trace->get_active_row_number();
	if (type >= trace_n)
		return;
	if (prefs.trace.get() == type)
		return;
	Setter s;
	s.set(prefs.trace, type);
	send(P_UPDATE_TRACE);
	write_prefs();
}

void Prefs::on_p_changed() {
	Setter s;
	s.set(prefs.p, scale_p->get_value());
	write_prefs();
}

void Prefs::on_p_default() {
	scale_p->set_value(default_p);
}

void Prefs::on_selected(std::string &str) {
	bool is_new;
	{
		Setter s;
		is_new = s.write_ref(prefs.exceptions).insert(str).second;
	}
	if (is_new) {
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
		Setter s;
		s.write_ref(prefs.exceptions).erase((Glib::ustring)((*iter)[cols.col]));
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
	Window w = Select_Window(dpy, True);
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
	Setter s;
	blabel->set_text(s.ref(prefs.button).get_button_text());
}
