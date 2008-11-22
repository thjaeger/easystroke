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

class Check : private Base {
	IO<bool> &io;
	Gtk::CheckButton *check;
	virtual void notify() { check->set_active(io.get()); }
	void on_changed() {
		bool b = check->get_active();
		if (b == io.get()) return;
		io.set(b);
	}
public:
	Check(IO<bool> &io_, const Glib::ustring &name) : io(io_) {
		io.connect(this);
		widgets->get_widget(name, check);
		notify();
		check->signal_toggled().connect(sigc::mem_fun(*this, &Check::on_changed));
	}
};

class Spin : private Base {
	IO<int> &io;
	Gtk::SpinButton *spin;
	Gtk::Button *button;
	virtual void notify() { spin->set_value(io.get()); }
	void on_changed() {
		int i = spin->get_value();
		if (i == io.get()) return;
		io.set(i);
	}
public:
	Spin(IO<int> &io_, const Glib::ustring & name) : io(io_) {
		io.connect(this);
		widgets->get_widget(name, spin);
		notify();
		spin->signal_value_changed().connect(sigc::mem_fun(*this, &Spin::on_changed));
	}
};

class Color : private Base {
	IO<unsigned long> &io;
	Gtk::ColorButton *color;
	virtual void notify() {
		unsigned long c = io.get();
		Gdk::Color col = color->get_color();
		col.set_rgb(257*(c >> 16), 257*((c >> 8) % 256), 257*(c % 256));
		color->set_color(col);
	}
	void on_changed() {
		Gdk::Color col = color->get_color();
		unsigned long c = ((col.get_red()/257)<<16) + ((col.get_green()/257)<<8) + col.get_blue()/257;
		if (c == io.get()) return;
		io.set(c);
	}
public:
	Color(IO<unsigned long> &io_, const Glib::ustring &name) : io(io_) {
		io.connect(this);
		widgets->get_widget(name, color);
		notify();
		color->signal_color_set().connect(sigc::mem_fun(*this, &Color::on_changed));
	}
};

class Combo : private Base {
	IO<int> &io;
	Gtk::ComboBox *combo;
	virtual void notify() { combo->set_active(io.get()); }
	void on_changed() {
		int i = combo->get_active_row_number();
		if (i < 0 || i == io.get()) return;
		io.set(i);
	}
public:
	Combo(IO<int> &io_, const Glib::ustring &name) : io(io_) {
		io.connect(this);
		widgets->get_widget(name, combo);
		notify();
		combo->signal_changed().connect(sigc::mem_fun(*this, &Combo::on_changed));
	}
};

template <class T> class ButtonSet {
	IO<T> &io;
	T def;
	void on_click() { io.set(def); }
public:
	ButtonSet(IO<T> &io_, const Glib::ustring &name, T def_) : io(io_), def(def_) {
		Gtk::Button *button;
		widgets->get_widget(name, button);
		button->signal_clicked().connect(sigc::mem_fun(*this, &ButtonSet::on_click));
	}
};

class Sensitive : private Base {
	Gtk::Widget *widget;
	Out<bool> &in;
public:
	virtual void notify() { widget->set_sensitive(in.get()); }
	Sensitive(Out<bool> &in_, const Glib::ustring &name) : in(in_) {
		in.connect(this);
		widgets->get_widget(name, widget);
		notify();
	}
};

bool and_(bool x, bool y) { return x && y; }
bool is_custom(int profile) { return profile == TO_CUSTOM; }
bool draw_line(TraceType t) { return t == TraceStandard || t == TraceShape; }

#define TRACE_NONE 0
#define TRACE_SHAPE 1
#define TRACE_LEGACY 2
#define TRACE_ANNOTATE 3
#define TRACE_FIRE 4
#define TRACE_WATER 5

int trace_to_int(TraceType t) {
	switch (t) {
		case TraceShape:
			return TRACE_SHAPE;
		case TraceStandard:
			return TRACE_LEGACY;
		case TraceAnnotate:
			return TRACE_ANNOTATE;
		case TraceFire:
			return TRACE_FIRE;
		case TraceWater:
			return TRACE_WATER;
		default:
			return TRACE_NONE;
	}
}

TraceType int_to_trace(int i) {
	switch (i) {
		case TRACE_SHAPE:
			return TraceShape;
		case TRACE_LEGACY:
			return TraceStandard;
		case TRACE_ANNOTATE:
			return TraceAnnotate;
		case TRACE_FIRE:
			return TraceFire;
		case TRACE_WATER:
			return TraceWater;
		default:
			return TraceNone;
	}
}

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

	new Check(prefs.tray_icon, "check_tray_icon");

	new Spin(prefs.init_timeout, "spin_timeout");
	new Spin(prefs.min_speed, "spin_min_speed");

	new ButtonSet<int>(prefs.radius, "button_default_radius", default_radius);
	new ButtonSet<int>(prefs.pressure_threshold, "button_default_pressure_threshold", default_pressure_threshold);

	new Combo(*new Bijection<TraceType, int>(&trace_to_int, &int_to_trace, prefs.trace), "combo_trace");
	new Color(prefs.color, "button_color");
	new Combo(prefs.timeout_profile, "combo_timeout");

	new Check(prefs.timeout_gestures, "check_timeout_gestures");

	Gtk::Button *bbutton, *add_exception, *remove_exception, *button_default_p;
	widgets->get_widget("button_add_exception", add_exception);
	widgets->get_widget("button_button", bbutton);
	widgets->get_widget("button_default_p", button_default_p);
	widgets->get_widget("button_remove_exception", remove_exception);
	widgets->get_widget("label_button", blabel);
	widgets->get_widget("treeview_exceptions", tv);
	widgets->get_widget("treeview_devices", dtv);
	widgets->get_widget("scale_p", scale_p);

	new Sensitive(xinput_v, "check_timing_workaround");
	new Sensitive(xinput_v, "check_ignore_grab");
	new Sensitive(xinput_v, "hbox_timeout_profile");
	new Sensitive(xinput_v, "check_timeout_gestures");
	new Sensitive(xinput_v, "treeview_devices");
	new Sensitive(*fun2(&and_, xinput_v, *fun(&is_custom, prefs.timeout_profile)), "hbox_timeout");
	new Sensitive(supports_pressure, "hbox_pressure");
	new Sensitive(supports_proximity, "check_proximity");
	new Sensitive(*fun(&draw_line, prefs.trace), "button_color");

	tm = Gtk::ListStore::create(cols);
	tv->set_model(tm);
	tv->append_column("Application (WM__CLASS)", cols.app);
	tm->set_sort_column(cols.app, Gtk::SORT_ASCENDING);

	CellRendererTextish *button_renderer = Gtk::manage(new CellRendererTextish);
	button_renderer->mode = CellRendererTextish::POPUP;
	tv->append_column("Button", *button_renderer);
	Gtk::TreeView::Column *col_button = tv->get_column(1);
	col_button->add_attribute(button_renderer->property_text(), cols.button);
	button_renderer->property_editable() = true;
	button_renderer->signal_editing_started().connect(sigc::mem_fun(*this, &Prefs::on_button_editing_started));

	bbutton->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_select_button));

	add_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_add));
	remove_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_remove));

	dtm = Gtk::ListStore::create(dcs);
	dtv->set_model(dtm);
	dtv->append_column_editable("Enabled", dcs.enabled);
	dtv->append_column("Device", dcs.name);
	dtm->signal_row_changed().connect(sigc::mem_fun(*this, &Prefs::on_device_toggled));
	update_device_list();

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
		remove_last_entry("combo_timeout");
	}
	set_button_label();

	const std::map<std::string, RButtonInfo> &exceptions = prefs.exceptions.ref();
	for (std::map<std::string, RButtonInfo>::const_iterator i = exceptions.begin(); i!=exceptions.end(); i++) {
		Gtk::TreeModel::Row row = *(tm->append());
		row[cols.app] = i->first;
		row[cols.button] = i->second ? i->second->get_button_text() : "<App disabled>";
	}
}

void Prefs::update_device_list() {
	if (!grabber->xinput)
		return;
	ignore_device_toggled = true;
	dtm->clear();
	for (int i = 0; i < grabber->xi_devs_n; i++) {
		std::string name = grabber->xi_devs[i]->name;
		Gtk::TreeModel::Row row = *(dtm->append());
		row[dcs.enabled] = !prefs.excluded_devices.get().count(name);
		row[dcs.name] = name;
	}
	ignore_device_toggled = false;
}

struct Prefs::SelectRow {
	Prefs *parent;
	std::string name;
	bool test(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter) {
		if ((*iter)[parent->cols.app] == name) {
			parent->tv->set_cursor(path);
			return true;
		}
		return false;
	}
};

SelectButton::SelectButton(ButtonInfo bi, bool def, bool any) {
	widgets->get_widget("dialog_select", dialog);
	widgets->get_widget("eventbox", eventbox);
	widgets->get_widget("toggle_shift", toggle_shift);
	widgets->get_widget("toggle_alt", toggle_alt);
	widgets->get_widget("toggle_control", toggle_control);
	widgets->get_widget("toggle_super", toggle_super);
	widgets->get_widget("toggle_any", toggle_any);
	widgets->get_widget("select_button", select_button);
	select_button->set_active(bi.button-1);
	toggle_shift->set_active(bi.button && (bi.state & GDK_SHIFT_MASK));
	toggle_control->set_active(bi.button && (bi.state & GDK_CONTROL_MASK));
	toggle_alt->set_active(bi.button && (bi.state & GDK_MOD1_MASK));
	toggle_super->set_active(bi.button && (bi.state & GDK_SUPER_MASK));
	toggle_any->set_active(any && bi.button && bi.state == AnyModifier);
	toggle_any->set_sensitive(any);

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
	handler[0] = eventbox->signal_button_press_event().connect(sigc::mem_fun(*this, &SelectButton::on_button_press));
	handler[1] = toggle_any->signal_toggled().connect(sigc::mem_fun(*this, &SelectButton::on_any_toggled));
	on_any_toggled();
}

SelectButton::~SelectButton() {
	handler[0].disconnect();
	handler[1].disconnect();
}

bool SelectButton::run() {
	grabber->suspend();
	dialog->show();
	Gtk::Button *select_ok;
	widgets->get_widget("select_ok", select_ok);
	select_ok->grab_focus();
	int response;
	do {
		response = dialog->run();
	} while (!response);
	dialog->hide();
	grabber->resume();
	switch (response) {
		case 1: // Okay
			event.button = select_button->get_active_row_number() + 1;
			if (!event.button)
				return false;
			event.state = 0;
			if (toggle_any->get_active()) {
				event.state = AnyModifier;
				return true;
			}
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
			event.button = 0;
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
	event.button = ev->button;
	event.state  = ev->state;
	if (toggle_any->get_active()) {
		event.state = AnyModifier;
	} else {
		if (event.state & Mod4Mask)
			event.state |= GDK_SUPER_MASK;
		event.state &= gtk_accelerator_get_default_mod_mask();
	}
	dialog->response(3);
	return true;
}

void SelectButton::on_any_toggled() {
	bool any = toggle_any->get_active(); 
	toggle_shift->set_sensitive(!any);
	toggle_control->set_sensitive(!any);
	toggle_alt->set_sensitive(!any);
	toggle_super->set_sensitive(!any);
}

void Prefs::on_select_button() {
	Atomic a;
	ButtonInfo &bi = prefs.button.write_ref(a);
	SelectButton sb(bi, true, true);
	if (!sb.run())
		return;
	bi = sb.event.button ? sb.event : default_button;
	grabber->update_button(bi);
	set_button_label();
}

void Prefs::on_p_changed() {
	prefs.p.set(scale_p->get_value());
}

void Prefs::on_p_default() {
	scale_p->set_value(default_p);
}

extern void update_current();

void select_window(sigc::slot<void, std::string> f);

void Prefs::on_add() {
	select_window(sigc::mem_fun(*this, &Prefs::on_selected));
}

void Prefs::on_selected(std::string str) {
	bool is_new;
	{
		Atomic a;
		is_new = prefs.exceptions.write_ref(a).insert(
				std::pair<std::string, RButtonInfo>(str, RButtonInfo())).second;
	}
	if (is_new) {
		Gtk::TreeModel::Row row = *(tm->append());
		row[cols.app] = str;
		row[cols.button] = "<App disabled>";
		Gtk::TreePath path = tm->get_path(row);
		tv->set_cursor(path);
	} else {
		SelectRow cb;
		cb.name = str;
		cb.parent = this;
		tm->foreach(sigc::mem_fun(cb, &SelectRow::test));
	}
}

void Prefs::on_button_editing_started(Gtk::CellEditable* editable, const Glib::ustring& path) {
	Gtk::TreeRow row(*tm->get_iter(path));
	Glib::ustring app = row[cols.app];
	ButtonInfo bi;
	bi.button = 0;
	bi.state = 0;
	std::map<std::string, RButtonInfo>::const_iterator i = prefs.exceptions.ref().find(app);
	if (i != prefs.exceptions.ref().end() && i->second)
		bi = *i->second;
	SelectButton sb(bi, true, true);
	if (!sb.run())
		return;
	RButtonInfo bi2;
	if (sb.event.button)
		bi2.reset(new ButtonInfo(sb.event));
	row[cols.button] = bi2 ? bi2->get_button_text() : "<App disabled>";
	Atomic a;
	prefs.exceptions.write_ref(a)[app] = bi2; 
}


void Prefs::on_remove() {
	Gtk::TreePath path;
	Gtk::TreeViewColumn *col;
	tv->get_cursor(path, col);
	if (path.gobj() != 0) {
		Gtk::TreeIter iter = *tm->get_iter(path);
		Atomic a;
		prefs.exceptions.write_ref(a).erase((Glib::ustring)((*iter)[cols.app]));
		tm->erase(iter);
		update_current();
	}
}

void Prefs::on_device_toggled(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter) {
	if (ignore_device_toggled)
		return;
	Atomic a;
	std::set<std::string> &ex = prefs.excluded_devices.write_ref(a);
	Glib::ustring device = (*iter)[dcs.name];
	grabber->xi_suspend();
	if ((*iter)[dcs.enabled])
		ex.erase(device);
	else
		ex.insert(device);
	grabber->xi_resume();
}

void Prefs::set_button_label() {
	blabel->set_text(prefs.button.ref().get_button_text());
}
