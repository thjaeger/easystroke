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

VarI<bool> xinput_v = false;
VarI<bool> supports_pressure = false;
VarI<bool> supports_proximity = false;

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

class Check : public IO<bool> {
	Gtk::CheckButton *check;
	virtual void notify(bool &x, Out<bool> *) { check->set_active(x); }
	void on_changed() {
		update(check->get_active());
		write_prefs();
	}
public:
	Check(const Glib::ustring &name) {
		widgets->get_widget(name, check);
		check->signal_toggled().connect(sigc::mem_fun(*this, &Check::on_changed));
	}
};

class Spin : public IO<int> {
	Gtk::SpinButton *spin;
	Gtk::Button *button;
	virtual void notify(int &x, Out<int> *) { spin->set_value(x); }
	void on_changed() {
		update(spin->get_value());
		write_prefs();
	}
public:
	Spin(const Glib::ustring & name) {
		widgets->get_widget(name, spin);
		spin->signal_value_changed().connect(sigc::mem_fun(*this, &Spin::on_changed));
	}
};

template <class T> class ButtonSet {
	VarI<T> &v;
	T def;
	void on_click() { v.set(def); }
public:
	ButtonSet(const Glib::ustring & name, VarI<T> &v_, T def_) : v(v_), def(def_) {
		Gtk::Button *button;
		widgets->get_widget(name, button);
		button->signal_clicked().connect(sigc::mem_fun(*this, &ButtonSet::on_click));
	}
};

class Sensitive : public In<bool> {
	Gtk::Widget *widget;
public:
	virtual void notify(bool &x, Out<bool> *) { widget->set_sensitive(x); }
	Sensitive(const Glib::ustring & name) {
		widgets->get_widget(name, widget);
	}
};

Prefs::Prefs() :
	q(sigc::mem_fun(*this, &Prefs::on_selected))
{
	prefs.advanced_ignore.identify(new Check("check_advanced_ignore"));
	prefs.ignore_grab.identify(new Check("check_ignore_grab"));
	prefs.timing_workaround.identify(new Check("check_timing_workaround"));
	prefs.show_clicks.identify(new Check("check_show_clicks"));

	prefs.radius.identify(new Spin("spin_radius"));

	prefs.pressure_abort.identify(new Check("check_pressure_abort"));
	prefs.pressure_threshold.identify(new Spin("spin_pressure_threshold"));
	prefs.pressure_abort.connect(new Sensitive("spin_pressure_threshold"));
	prefs.pressure_abort.connect(new Sensitive("button_default_pressure_threshold"));

	prefs.proximity.identify(new Check("check_proximity"));

	new ButtonSet<int>("button_default_radius", prefs.radius, default_radius);
	new ButtonSet<int>("button_default_pressure_threshold", prefs.pressure_threshold, default_pressure_threshold);

	Gtk::Button *bbutton, *add_exception, *remove_exception, *button_default_p;
	widgets->get_widget("button_add_exception", add_exception);
	widgets->get_widget("button_button", bbutton);
	widgets->get_widget("button_default_p", button_default_p);
	widgets->get_widget("button_remove_exception", remove_exception);
	widgets->get_widget("combo_trace", trace);
	widgets->get_widget("label_button", blabel);
	widgets->get_widget("treeview_exceptions", tv);
	widgets->get_widget("scale_p", scale_p);

	xinput_v.connect(new Sensitive("check_timing_workaround"));
	xinput_v.connect(new Sensitive("check_ignore_grab"));
	supports_pressure.connect(new Sensitive("hbox_pressure"));
	supports_proximity.connect(new Sensitive("check_proximity"));

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

	Atomic a;
	std::set<std::string> exceptions = prefs.exceptions.ref(a);
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
	Atomic a;
	ButtonInfo &bi = prefs.button.write_ref(a);
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
	prefs.trace.set(type);
	send(P_UPDATE_TRACE);
	write_prefs();
}

void Prefs::on_p_changed() {
	prefs.p.set(scale_p->get_value());
	write_prefs();
}

void Prefs::on_p_default() {
	scale_p->set_value(default_p);
}

void Prefs::on_selected(std::string &str) {
	bool is_new;
	{
		Atomic a;
		is_new = prefs.exceptions.write_ref(a).insert(str).second;
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
		Atomic a;
		prefs.exceptions.write_ref(a).erase((Glib::ustring)((*iter)[cols.col]));
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
	Atomic a;
	blabel->set_text(prefs.button.ref(a).get_button_text());
}
