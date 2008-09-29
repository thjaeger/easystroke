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
#include "prefs.h"
#include "win.h"
#include "main.h"
#include "grabber.h"

#include <X11/Xutil.h>

#include <set>
#include <iostream>

Source<bool> xinput_v(false);
Source<bool> supports_pressure(false);
Source<bool> supports_proximity(false);

class Check : public In<bool> {
	IO<bool> &io;
	Gtk::CheckButton *check;
	virtual void notify(const bool v) { check->set_active(v); }
	void on_changed() {
		bool b = check->get_active();
		if (b == io.get()) return;
		io.set(b);
	}
public:
	Check(IO<bool> &io_, const Glib::ustring &name) : In<bool>(io_), io(io_) {
		widgets->get_widget(name, check);
		notify(io.get());
		check->signal_toggled().connect(sigc::mem_fun(*this, &Check::on_changed));
	}
};

class Spin : public In<int> {
	IO<int> &io;
	Gtk::SpinButton *spin;
	Gtk::Button *button;
	virtual void notify(const int x) { spin->set_value(x); }
	void on_changed() {
		int i = spin->get_value();
		if (i == io.get()) return;
		io.set(i);
	}
public:
	Spin(IO<int> &io_, const Glib::ustring & name) : In<int>(io_), io(io_) {
		widgets->get_widget(name, spin);
		notify(io.get());
		spin->signal_value_changed().connect(sigc::mem_fun(*this, &Spin::on_changed));
	}
};

class Combo : public In<int> {
	IO<int> &io;
	Gtk::ComboBox *combo;
	virtual void notify(const int v) { combo->set_active(v); }
	void on_changed() {
		int i = combo->get_active_row_number();
		if (i < 0 || i == io.get()) return;
		io.set(i);
	}
public:
	Combo(IO<int> &io_, const Glib::ustring & name) : In<int>(io_), io(io_) {
		widgets->get_widget(name, combo);
		notify(io.get());
		combo->signal_changed().connect(sigc::mem_fun(*this, &Combo::on_changed));
	}
};

template <class T> class ButtonSet {
	IO<T> &io;
	T def;
	void on_click() { io.set(def); }
public:
	ButtonSet(IO<T> &io_, const Glib::ustring & name, T def_) : io(io_), def(def_) {
		Gtk::Button *button;
		widgets->get_widget(name, button);
		button->signal_clicked().connect(sigc::mem_fun(*this, &ButtonSet::on_click));
	}
};

class Sensitive : public In<bool> {
	Gtk::Widget *widget;
public:
	virtual void notify(const bool v) { widget->set_sensitive(v); }
	Sensitive(Value<bool> &in, const Glib::ustring & name) : In<bool>(in) {
		widgets->get_widget(name, widget);
		notify(in.get());
	}
};

class ToIsCustom : public Fun<int, bool> {
	bool run(const int &profile) { return profile == TO_CUSTOM; }
public:
	ToIsCustom(Value<int> &in) : Fun<int, bool>(in) {}
};

class TimeoutProfile : public In<int> {
public:
	virtual void notify(const int v) {
		switch (v) {
			case TO_OFF:
				prefs.init_timeout.set(0);
				prefs.min_speed.set(0);
				break;
			case TO_CONSERVATIVE:
				prefs.init_timeout.set(150);
				prefs.min_speed.set(50);
				break;
			case TO_MEDIUM:
				prefs.init_timeout.set(30);
				prefs.min_speed.set(80);
				break;
			case TO_AGGRESSIVE:
				prefs.init_timeout.set(15);
				prefs.min_speed.set(150);
				break;
			case TO_FLICK:
				prefs.init_timeout.set(20);
				prefs.min_speed.set(500);
				break;
		}
	}
	TimeoutProfile(Value<int> &in) : In<int>(in) {
		notify(in.get());
	}
};

void remove_last_entry(const Glib::ustring & name) {
	Gtk::ComboBox *combo;
	widgets->get_widget(name, combo);
	Glib::RefPtr<Gtk::ListStore> combo_model = Glib::RefPtr<Gtk::ListStore>::cast_dynamic(combo->get_model());
	Gtk::TreeIter i = combo_model->children().end();
	combo_model->erase(--i);
}

Prefs::Prefs() {
 	new Check(prefs.advanced_ignore, "check_advanced_ignore");
	new Check(prefs.ignore_grab, "check_ignore_grab");
	new Check(prefs.timing_workaround, "check_timing_workaround");
	new Check(prefs.show_clicks, "check_show_clicks");

	new Spin(prefs.radius, "spin_radius");

	new Check(prefs.pressure_abort, "check_pressure_abort");
	new Spin(prefs.pressure_threshold, "spin_pressure_threshold");
	new Sensitive(prefs.pressure_abort, "spin_pressure_threshold");
	new Sensitive(prefs.pressure_abort, "button_default_pressure_threshold");

	new Check(prefs.proximity, "check_proximity");

	new Check(prefs.feedback, "check_feedback");
	new Check(prefs.left_handed, "check_left_handed");
	new Sensitive(prefs.feedback, "check_left_handed");

	new Spin(prefs.init_timeout, "spin_timeout");
	new Spin(prefs.min_speed, "spin_min_speed");

	new ButtonSet<int>(prefs.radius, "button_default_radius", default_radius);
	new ButtonSet<int>(prefs.pressure_threshold, "button_default_pressure_threshold", default_pressure_threshold);

	new Combo(*new Converter<TraceType, int>(prefs.trace), "combo_trace");
	new Combo(prefs.timeout_profile, "combo_timeout");
	new Sensitive(*new ToIsCustom(prefs.timeout_profile), "hbox_timeout");
	new TimeoutProfile(prefs.timeout_profile);

	new Check(prefs.timeout_gestures, "check_timeout_gestures");

	Gtk::Button *bbutton, *add_exception, *remove_exception, *button_default_p;
	widgets->get_widget("button_add_exception", add_exception);
	widgets->get_widget("button_button", bbutton);
	widgets->get_widget("button_default_p", button_default_p);
	widgets->get_widget("button_remove_exception", remove_exception);
	widgets->get_widget("label_button", blabel);
	widgets->get_widget("treeview_exceptions", tv);
	widgets->get_widget("scale_p", scale_p);

	new Sensitive(xinput_v, "check_timing_workaround");
	new Sensitive(xinput_v, "check_ignore_grab");
	new Sensitive(xinput_v, "hbox_timeout_profile");
	new Sensitive(xinput_v, "hbox_timeout");
	new Sensitive(supports_pressure, "hbox_pressure");
	new Sensitive(supports_proximity, "check_proximity");

	tm = Gtk::ListStore::create(cols);
	tv->set_model(tm);
	tv->append_column("WM__CLASS name", cols.col);
	tm->set_sort_column(cols.col, Gtk::SORT_ASCENDING);

	bbutton->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_select_button));

	add_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_add));
	remove_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_remove));

	if (!experimental) {
		remove_last_entry("combo_trace");
		remove_last_entry("combo_timeout");
	}

	double p = prefs.p.get();
	scale_p->set_value(p);
	scale_p->signal_value_changed().connect(sigc::mem_fun(*this, &Prefs::on_p_changed));
	button_default_p->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_p_default));

	if (!experimental) {
		Gtk::HBox *hbox;
	       	widgets->get_widget("hbox_algo", hbox);
		hbox->hide();
	       	widgets->get_widget("hbox_timeout", hbox);
		hbox->hide();
	}
	set_button_label();

	std::set<std::string> exceptions = prefs.exceptions.ref();
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
	grabber->suspend();
	int response;
	do {
		response = dialog->run();
	} while (response == 0);
	dialog->hide();
	grabber->resume();
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
	grabber->regrab();
	set_button_label();
}

void Prefs::on_p_changed() {
	prefs.p.set(scale_p->get_value());
}

void Prefs::on_p_default() {
	scale_p->set_value(default_p);
}

extern void update_current();

void Prefs::on_selected(std::string str) {
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
		update_current();
	}
}

void Prefs::set_button_label() {
	blabel->set_text(prefs.button.ref().get_button_text());
}
