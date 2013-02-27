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
#include "prefs.h"
#include "win.h"
#include "main.h"
#include "grabber.h"
#include <glibmm/i18n.h>
#include <sys/stat.h>
#include "cellrenderertextish.h"

#include <set>
#include <iostream>

extern const char *desktop_file;

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

template <class T> class Adjustment : private Base {
	IO<T> &io;
	Glib::RefPtr<Gtk::Adjustment> adj;
	Gtk::Button *button;
	virtual void notify() { adj->set_value(io.get()); }
	void on_changed() {
		T i = (T)adj->get_value();
		if (i == io.get()) return;
		io.set(i);
	}
public:
	Adjustment(IO<T> &io_, const Glib::ustring & name) : io(io_) {
		io.connect(this);
		adj = Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(widgets->get_object(name));
		notify();
		adj->signal_value_changed().connect(sigc::mem_fun(*this, &Adjustment::on_changed));
	}
};

class Color : private Base {
	IO<RGBA> &io;
	Gtk::ColorButton *color;
	virtual void notify() {
		color->set_color(io.get().color);
		color->set_alpha(io.get().alpha);
	}
	void on_changed() {
		RGBA rgba;
		rgba.color = color->get_color();
		rgba.alpha = color->get_alpha();
		if (rgba == io.get()) return;
		io.set(rgba);
	}
public:
	Color(IO<RGBA> &io_, const Glib::ustring &name) : io(io_) {
		io.connect(this);
		widgets->get_widget(name, color);
		color->set_use_alpha();
		notify();
		color->signal_color_set().connect(sigc::mem_fun(*this, &Color::on_changed));
	}
};

template <class T> class Combo : private Base {
public:
	struct Info {
		T value;
		const char* name;
	};
private:
	IO<T> &io;
	const Info *info;
	Gtk::ComboBoxText *combo;
	virtual void notify() {
		T value = io.get();
		int i = 0;
		while (info[i].name && info[i].value != value) i++;
		combo->set_active(i);
	}
	void on_changed() {
		int row = combo->get_active_row_number();
		if (row < 0)
			return;
		T value = info[row].value;
		if (value == io.get())
			return;
		io.set(value);
	}
public:
	Combo(IO<T> &io_, const Glib::ustring &name, const Info *info_) : io(io_), info(info_) {
		io.connect(this);
		Gtk::Bin *parent;
		widgets->get_widget(name, parent);
		combo = Gtk::manage(new Gtk::ComboBoxText);
		parent->add(*combo);
		for (const Info *i = info; i->name; i++)
			combo->append(_(i->name));
		notify();
		combo->signal_changed().connect(sigc::mem_fun(*this, &Combo::on_changed));
		combo->show();
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

static bool is_custom(TimeoutType profile) { return profile == TimeoutCustom; }
static bool draw_trace(TraceType t) { return t == TraceDefault || t == TraceShape; }

const Combo<TraceType>::Info trace_info[] = {
	{ TraceNone, N_("None") },
	{ TraceDefault, N_("Default") },
	{ TraceShape, N_("XShape") },
	{ TraceAnnotate, N_("Annotate (compiz)") },
	{ TraceFire, N_("Fire (compiz)") },
	{ TraceWater, N_("Water (compiz)") },
	{ TraceDefault, 0 }
};

const Combo<TimeoutType>::Info timeout_info[] = {
	{ TimeoutOff, N_("Timeout Off") },
	{ TimeoutConservative, N_("Conservative") },
	{ TimeoutDefault, N_("Default") },
	{ TimeoutMedium, N_("Medium") },
	{ TimeoutAggressive, N_("Aggressive") },
	{ TimeoutFlick, N_("Flick") },
	{ TimeoutDefault, 0 }
};

const Combo<TimeoutType>::Info timeout_info_exp[] = {
	{ TimeoutOff, N_("Timeout Off") },
	{ TimeoutConservative, N_("Conservative") },
	{ TimeoutDefault, N_("Default") },
	{ TimeoutMedium, N_("Medium") },
	{ TimeoutAggressive, N_("Aggressive") },
	{ TimeoutFlick, N_("Flick") },
	{ TimeoutCustom, N_("Custom") },
	{ TimeoutDefault, 0 }
};

Source<bool> autostart_ok(true);

class Autostart : public IO<bool>, private Base {
	bool a;
	std::string filename;
public:
	Autostart() {
		std::string dir = getenv("HOME");
		dir += "/.config/autostart";
		filename = dir + "/easystroke.desktop";

		if (!is_dir(dir) && mkdir(dir.c_str(), 0777)) {
			autostart_ok.set(false);
			return;
		}
		a = is_file(filename);
	}
	virtual void set(const bool a_) {
		a = a_;
		notify();
	}
	virtual bool get() const { return a; }
	virtual void notify() {
		if (a) {
			char path[256] = "easystroke";
			readlink("/proc/self/exe", path, sizeof(path));

			FILE *file = fopen(filename.c_str(), "w");
			if (!file || fprintf(file, desktop_file, path) == -1)
				autostart_ok.set(false);
			if (file)
				fclose(file);
		} else {
			if (remove(filename.c_str()) == -1)
				autostart_ok.set(false);
		}
		update();
	}
};

Autostart autostart;

void remove_last_entry(const Glib::ustring & name) {
	Gtk::ComboBox *combo;
	widgets->get_widget(name, combo);
	Glib::RefPtr<Gtk::ListStore> combo_model = Glib::RefPtr<Gtk::ListStore>::cast_dynamic(combo->get_model());
	Gtk::TreeIter i = combo_model->children().end();
	combo_model->erase(--i);
}

static void on_prefs_editing_started(GtkCellRenderer *, GtkCellEditable *editable, const gchar *path, gpointer data) {
	((Prefs *)data)->on_button_editing_started(editable, path);
}

Prefs::Prefs() {
	new Check(prefs.advanced_ignore, "check_advanced_ignore");

	new Check(prefs.proximity, "check_proximity");

	new Check(prefs.feedback, "check_feedback");
	new Check(prefs.left_handed, "check_left_handed");
	new Sensitive(prefs.feedback, "check_left_handed");
	new Check(prefs.advanced_popups, "check_advanced_popups");
	new Sensitive(prefs.feedback, "check_advanced_popups");

	new Check(prefs.tray_icon, "check_tray_icon");
	new Sensitive(prefs.tray_icon, "check_tray_feedback");
	new Check(prefs.tray_feedback, "check_tray_feedback");

	new Check(autostart, "check_autostart");
	new Sensitive(autostart_ok, "check_autostart");

	new Adjustment<int>(prefs.init_timeout, "adjustment_init_timeout");
	new Adjustment<int>(prefs.final_timeout, "adjustment_final_timeout");

	new Combo<TraceType>(prefs.trace, "box_trace", trace_info);
	new Color(prefs.color, "button_color");
	new Adjustment<int>(prefs.trace_width, "adjustment_trace_width");
	new Combo<TimeoutType>(prefs.timeout_profile, "box_timeout", experimental ? timeout_info_exp : timeout_info);

	new Check(prefs.whitelist, "check_whitelist");
	new Check(prefs.timeout_gestures, "check_timeout_gestures");

	new Check(prefs.scroll_invert, "check_scroll_invert");
	new Adjustment<double>(prefs.scroll_speed, "adjustment_scroll_speed");

	new Check(prefs.move_back, "check_move_back");

	new Check(prefs.show_osd, "check_osd");

	new Check(prefs.touch, "check_touch");

	Gtk::Button *bbutton, *add_exception, *remove_exception, *add_extra, *edit_extra, *remove_extra;
	widgets->get_widget("button_add_exception", add_exception);
	widgets->get_widget("button_button", bbutton);
	widgets->get_widget("button_remove_exception", remove_exception);
	widgets->get_widget("label_button", blabel);
	widgets->get_widget("button_add_extra", add_extra);
	widgets->get_widget("button_edit_extra", edit_extra);
	widgets->get_widget("button_remove_extra", remove_extra);
	widgets->get_widget("treeview_exceptions", tv);
	widgets->get_widget("treeview_devices", dtv);
	widgets->get_widget("treeview_extra", etv);
	widgets->get_widget("frame_tablet", frame_tablet);

	new Sensitive(*fun(&is_custom, prefs.timeout_profile), "hbox_timeout");
	new Sensitive(*fun(&draw_trace, prefs.trace), "button_color");
	new Sensitive(*fun(&draw_trace, prefs.trace), "spin_trace_width");

	tm = Gtk::ListStore::create(cols);
	tv->set_model(tm);
	tv->append_column(_("Application (WM__CLASS)"), cols.user_app);
	tm->set_sort_column(cols.user_app, Gtk::SORT_ASCENDING);

	CellRendererTextish *button_renderer = cell_renderer_textish_new ();
	button_renderer->mode = CELL_RENDERER_TEXTISH_MODE_Popup;
	GtkTreeViewColumn *col_button = gtk_tree_view_column_new_with_attributes(_("Button"), GTK_CELL_RENDERER (button_renderer), "text", cols.button.index(), NULL);
	gtk_tree_view_append_column(tv->gobj(), col_button);
	g_object_set(button_renderer, "editable", true, NULL);
	g_signal_connect(button_renderer, "editing-started", G_CALLBACK(on_prefs_editing_started), this);

	bbutton->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_select_button));

	add_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_add));
	remove_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_remove));

	dtm = Gtk::ListStore::create(dcs);
	dtv->set_model(dtm);
	dtv->append_column_editable(_("Enabled"), dcs.enabled);
	int n = dtv->append_column(_("Device"), dcs.name);
	Gtk::TreeView::Column *col_device = dtv->get_column(n-1);
	col_device->set_expand();

	Glib::RefPtr<Gtk::ListStore> timeout_model = Gtk::ListStore::create(timeout_columns);
	{
		Gtk::TreeModel::Row row = *(timeout_model->append());
		row[timeout_columns.name] = _("<unchanged>");
	}
	for (const Combo<TimeoutType>::Info *i = timeout_info; i->name; i++) {
		Gtk::TreeModel::Row row = *(timeout_model->append());
		row[timeout_columns.name] = i->name;
	}

	Gtk::CellRendererCombo *timeout_renderer = Gtk::manage(new Gtk::CellRendererCombo);
	timeout_renderer->property_model() = timeout_model;
	timeout_renderer->property_editable() = true;
	timeout_renderer->property_text_column() = 0;
	timeout_renderer->property_has_entry() = false;
        timeout_renderer->signal_edited().connect(sigc::mem_fun(*this, &Prefs::on_device_timeout_changed));
	n = dtv->append_column(_("Timeout profile"), *timeout_renderer);
	Gtk::TreeView::Column *col_timeout = dtv->get_column(n-1);
	col_timeout->add_attribute(timeout_renderer->property_text(), dcs.timeout);

	dtm->signal_row_changed().connect(sigc::mem_fun(*this, &Prefs::on_device_toggled));
	update_device_list();

	add_extra->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_add_extra));
	edit_extra->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_edit_extra));
	remove_extra->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_remove_extra));

	etm = Gtk::ListStore::create(ecs);
	etv->set_model(etm);
	etv->append_column(_("Button"), ecs.str);
	update_extra_buttons();

	if (!experimental) {
		Gtk::HBox *hbox;
		widgets->get_widget("hbox_timeout", hbox);
		hbox->hide();
	}

	set_button_label();

	const std::map<std::string, RButtonInfo> &exceptions = prefs.exceptions.ref();
	for (std::map<std::string, RButtonInfo>::const_iterator i = exceptions.begin(); i!=exceptions.end(); i++) {
		Gtk::TreeModel::Row row = *(tm->append());
		row[cols.app] = i->first;
		row[cols.user_app] = app_name_hr(i->first);
		row[cols.button] = i->second ? i->second->get_button_text() : _("<App disabled>");
	}
}

void Prefs::update_device_list() {
	bool proximity = false;
	ignore_device_toggled = true;
	dtm->clear();
	std::set<std::string> names;
	for (Grabber::DeviceMap::iterator i = grabber->xi_devs.begin(); i != grabber->xi_devs.end(); ++i) {
		if (i->second->proximity_axis >= 0)
			proximity = true;
		std::string name = i->second->name;
		if (names.count(name))
			continue;
		names.insert(name);
		Gtk::TreeModel::Row row = *(dtm->append());
		row[dcs.enabled] = !prefs.excluded_devices.get().count(name);
		row[dcs.name] = name;
		row[dcs.timeout] = _("<unchanged>");

		const std::map<std::string, TimeoutType> &dt = prefs.device_timeout.ref();
		std::map<std::string, TimeoutType>::const_iterator j = dt.find(name);
		if (j != dt.end())
			for (const Combo<TimeoutType>::Info *i = timeout_info; i->name; i++)
				if (j->second == i->value)
					row[dcs.timeout] = i->name;
	}
	ignore_device_toggled = false;
	frame_tablet->set_visible(proximity);
}

void Prefs::update_extra_buttons() {
	etm->clear();
	std::vector<ButtonInfo> &extra = prefs.extra_buttons.unsafe_ref();
	for (std::vector<ButtonInfo>::iterator i = extra.begin(); i != extra.end(); i++) {
		Gtk::TreeModel::Row row = *(etm->append());
		row[ecs.str] = i->get_button_text();
		row[ecs.i] = i;
	}
}

void Prefs::on_add_extra() {
	ButtonInfo bi;
	SelectButton sb(bi, true, true);
	if (!sb.run())
		return;
	Atomic a;
	std::vector<ButtonInfo> &extra = prefs.extra_buttons.write_ref(a);
	for (std::vector<ButtonInfo>::iterator i = extra.begin(); i != extra.end();)
		if (i->overlap(sb.event))
			i = extra.erase(i);
		else
			i++;
	extra.push_back(sb.event);
	stable_sort(extra.begin(), extra.end());
	update_extra_buttons();
}

void Prefs::on_edit_extra() {
	Gtk::TreePath path;
	Gtk::TreeViewColumn *col;
	etv->get_cursor(path, col);
	if (!path.gobj())
		return;
	Gtk::TreeIter iter = *etm->get_iter(path);
	std::vector<ButtonInfo>::iterator i = (*iter)[ecs.i];
	SelectButton sb(*i, true, true);
	if (!sb.run())
		return;
	Atomic a;
	std::vector<ButtonInfo> &extra = prefs.extra_buttons.write_ref(a);
	for (std::vector<ButtonInfo>::iterator j = extra.begin(); j != extra.end();)
		if (j != i && j->overlap(sb.event))
			j = extra.erase(j);
		else
			j++;
	*i = sb.event;
	update_extra_buttons();
}

void Prefs::on_remove_extra() {
	Gtk::TreePath path;
	Gtk::TreeViewColumn *col;
	etv->get_cursor(path, col);
	if (!path.gobj())
		return;
	Gtk::TreeIter iter = *etm->get_iter(path);
	Atomic a;
	std::vector<ButtonInfo>::iterator i = (*iter)[ecs.i];
	prefs.extra_buttons.write_ref(a).erase(i);
	update_extra_buttons();
}

SelectButton::SelectButton(ButtonInfo bi, bool def, bool any) {
	widgets->get_widget("dialog_select", dialog);
	dialog->set_message(_("Select a Mouse or Pen Button"));
	dialog->set_secondary_text(_("Please place your mouse or pen in the box below and press the button that you want to select.  You can also hold down additional modifiers."));
	widgets->get_widget("eventbox", eventbox);
	widgets->get_widget("toggle_shift", toggle_shift);
	widgets->get_widget("toggle_alt", toggle_alt);
	widgets->get_widget("toggle_control", toggle_control);
	widgets->get_widget("toggle_super", toggle_super);
	widgets->get_widget("toggle_any", toggle_any);
	widgets->get_widget("radio_timeout_default", radio_timeout_default);
	widgets->get_widget("radio_instant", radio_instant);
	widgets->get_widget("radio_click_hold", radio_click_hold);
	Gtk::Bin *box_button;
	widgets->get_widget("box_button", box_button);
	Gtk::HBox *hbox_button_timeout;
	widgets->get_widget("hbox_button_timeout", hbox_button_timeout);
	select_button = dynamic_cast<Gtk::ComboBoxText *>(box_button->get_child());
	if (!select_button) {
		select_button = Gtk::manage(new Gtk::ComboBoxText);
		box_button->add(*select_button);
		for (int i = 1; i <= 12; i++)
			select_button->append(Glib::ustring::compose(_("Button %1"), i));
		select_button->show();
	}
	select_button->set_active(bi.button-1);
	toggle_shift->set_active(bi.button && (bi.state & GDK_SHIFT_MASK));
	toggle_control->set_active(bi.button && (bi.state & GDK_CONTROL_MASK));
	toggle_alt->set_active(bi.button && (bi.state & GDK_MOD1_MASK));
	toggle_super->set_active(bi.button && (bi.state & GDK_SUPER_MASK));
	toggle_any->set_active(any && bi.button && bi.state == AnyModifier);
	if (any) {
		hbox_button_timeout->show();
		toggle_any->show();
	} else {
		hbox_button_timeout->hide();
		toggle_any->hide();
	}
	if (bi.instant)
		radio_instant->set_active();
	else if (bi.click_hold)
		radio_click_hold->set_active();
	else
		radio_timeout_default->set_active();

	Gtk::Button *select_default;
	widgets->get_widget("select_default", select_default);
	if (def)
		select_default->show();
	else
		select_default->hide();

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
	grabber->queue_suspend();
	dialog->show();
	Gtk::Button *select_ok;
	widgets->get_widget("select_ok", select_ok);
	select_ok->grab_focus();
	int response;
	do {
		response = dialog->run();
	} while (!response);
	dialog->hide();
	grabber->queue_resume();
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
			event.instant = radio_instant->get_active();
			event.click_hold = radio_click_hold->get_active();
			return true;
		case 2: // Default
			event.button = 0;
			event.state = 0;
			event.instant = false;
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
	event.instant = radio_instant->get_active();
	event.click_hold = radio_click_hold->get_active();
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
	set_button_label();
}

bool Prefs::select_row(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter, std::string name) {
	if ((std::string)(*iter)[cols.app] == name) {
		tv->set_cursor(path);
		return true;
	}
	return false;
}

void Prefs::on_add() {
	std::string str = grabber->select_window();
	bool is_new;
	{
		Atomic a;
		is_new = prefs.exceptions.write_ref(a).insert(
				std::pair<std::string, RButtonInfo>(str, RButtonInfo())).second;
	}
	if (is_new) {
		Gtk::TreeModel::Row row = *(tm->append());
		row[cols.app] = str;
		row[cols.user_app] = app_name_hr(str);
		row[cols.button] = _("<App disabled>");
		Gtk::TreePath path = tm->get_path(row);
		tv->set_cursor(path);
	} else {
		tm->foreach(sigc::bind(sigc::mem_fun(*this, &Prefs::select_row), str));
	}
}

void Prefs::on_button_editing_started(GtkCellEditable* editable, const gchar *path) {
	Gtk::TreeRow row(*tm->get_iter(path));
	std::string app = row[cols.app];
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
	row[cols.button] = bi2 ? bi2->get_button_text() : _("<App disabled>");
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
	}
}

void Prefs::on_device_toggled(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter) {
	if (ignore_device_toggled)
		return;
	Atomic a;
	std::set<std::string> &ex = prefs.excluded_devices.write_ref(a);
	Glib::ustring device = (*iter)[dcs.name];
	if ((*iter)[dcs.enabled])
		ex.erase(device);
	else
		ex.insert(device);
}

void Prefs::on_device_timeout_changed(const Glib::ustring& path, const Glib::ustring& new_text) {
	Gtk::TreeRow row(*dtm->get_iter(path));
	row[dcs.timeout] = new_text;
	Glib::ustring device = row[dcs.name];
	Atomic a;

	for (const Combo<TimeoutType>::Info *i = timeout_info; i->name; i++) {
		if (Glib::ustring(i->name) == new_text) {
			std::map<std::string, TimeoutType> &dt = prefs.device_timeout.write_ref(a);
			dt[device] = i->value;
			return;
		}
	}

	std::map<std::string, TimeoutType> &dt = prefs.device_timeout.write_ref(a);
	dt.erase(device);
}

void Prefs::set_button_label() {
	blabel->set_text(prefs.button.ref().get_button_text());
}
