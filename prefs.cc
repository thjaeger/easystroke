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

Glib::ustring get_button_text(ButtonInfo &bi);

Prefs::Prefs(Win *parent_) : 
	good_state(true), parent(parent_), q(sigc::mem_fun(*this, &Prefs::on_selected)),
	have_last_click(false), have_last_stroke_click(false) 
{
	Gtk::Button *bbutton, *add_exception, *remove_exception, *button_default_p, *button_default_delay;
	parent->widgets->get_widget("button_add_exception", add_exception);
	parent->widgets->get_widget("button_button", bbutton);
	parent->widgets->get_widget("button_default_delay", button_default_delay);
	parent->widgets->get_widget("button_default_p", button_default_p);
	parent->widgets->get_widget("button_remove_exception", remove_exception);
	parent->widgets->get_widget("combo_trace", trace);
	parent->widgets->get_widget("label_button", blabel);
	parent->widgets->get_widget("treeview_exceptions", tv);
	parent->widgets->get_widget("scale_p", scale_p);
	parent->widgets->get_widget("spin_delay", spin_delay);

	// I love glade.
	Gtk::Alignment *align;

	parent->widgets->get_widget("alignment_click", align);
	click = Gtk::manage(new Gtk::ComboBoxText());
	align->add(*click);
	click->append_text("Default");
	click->append_text("Ignore");
	click->append_text("Select...");
	click->show();

	parent->widgets->get_widget("alignment_stroke_click", align);
	stroke_click = Gtk::manage(new Gtk::ComboBoxText());
	align->add(*stroke_click);
	stroke_click->append_text("Abort Stroke");
	stroke_click->append_text("Ignore");
	stroke_click->append_text("Button 1");
	stroke_click->append_text("Button 2");
	stroke_click->append_text("Button 3");
	stroke_click->append_text("Button 8");
	stroke_click->append_text("Button 9");
	stroke_click->append_text("Select...");
	stroke_click->show();

	tm = Gtk::ListStore::create(cols);
	tv->set_model(tm);
	tv->append_column("WM__CLASS name", cols.col);
	tm->set_sort_column(cols.col, Gtk::SORT_ASCENDING);

	bbutton->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_select_button));

	add_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_add));
	remove_exception->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_remove));

	click->signal_changed().connect(sigc::mem_fun(*this, &Prefs::on_click_changed));
	show_click();

	stroke_click->signal_changed().connect(sigc::mem_fun(*this, &Prefs::on_stroke_click_changed));
	show_stroke_click();

	trace->signal_changed().connect(sigc::mem_fun(*this, &Prefs::on_trace_changed));
	trace->set_active(prefs().trace.get());

	double p = prefs().p.get();
	scale_p->set_value(p);
	scale_p->signal_value_changed().connect(sigc::mem_fun(*this, &Prefs::on_p_changed));
	button_default_p->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_p_default));

	spin_delay->set_value(prefs().delay.get());
	spin_delay->signal_value_changed().connect(sigc::mem_fun(*this, &Prefs::on_delay_changed));
	button_default_delay->signal_clicked().connect(sigc::mem_fun(*this, &Prefs::on_delay_default));

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

class SelectButton {
public:
	SelectButton(const Glib::RefPtr<Gtk::Builder> widgets, ButtonInfo bi) {
		widgets->get_widget("dialog_select", dialog);
		widgets->get_widget("eventbox", eventbox);
		widgets->get_widget("toggle_shift", toggle_shift);
		widgets->get_widget("toggle_alt", toggle_alt);
		widgets->get_widget("toggle_control", toggle_control);
		widgets->get_widget("toggle_super", toggle_super);
		widgets->get_widget("select_button", select_button);
		select_button->set_active(bi.special ? -1 : bi.button-1);
		toggle_shift->set_active(!bi.special && (bi.state & GDK_SHIFT_MASK));
		toggle_control->set_active(!bi.special && (bi.state & GDK_CONTROL_MASK));
		toggle_alt->set_active(!bi.special && (bi.state & GDK_MOD1_MASK));
		toggle_super->set_active(!bi.special && (bi.state & GDK_MOD4_MASK));
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

	~SelectButton() {
		handler.disconnect();
	}

	bool run() {
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
				event.button = 2;
				event.state = 0;
				return true;
			case 3: // Click - all the work has already been done
				return true;
			case -1: // Cancel
			default: // Something went wrong
				return false;
		}
	}
	GdkEventButton event;
private:
	Gtk::Dialog *dialog;
	bool on_button_press(GdkEventButton *ev) {
		event = *ev;
		dialog->response(3);
		return true;
	}
	Gtk::EventBox *eventbox;
	Gtk::ToggleButton *toggle_shift, *toggle_control, *toggle_alt, *toggle_super;
	Gtk::ComboBox *select_button;
	sigc::connection handler;
};

void Prefs::on_select_button() {
	SelectButton sb(parent->widgets, prefs().button.get());
	if (!sb.run())
		return;
	{
		Ref<ButtonInfo> ref(prefs().button);
		ref->button = sb.event.button;
		ref->state = sb.event.state;
		ref->special = 0;
	}
	send(P_REGRAB);
	set_button_label();
	write();
}

bool set_by_me = false; //TODO: Come up with something better

void Prefs::show_click() {
	Ref<ButtonInfo> ref(prefs().click);
	if (ref->special) {
		set_by_me = true;
		click->set_active(ref->special - 1);
		set_by_me = false;
		return;
	}
	set_by_me = true;
	if (have_last_click) {
		click->remove_text(get_button_text(last_click));
	}
	last_click = *ref;
	click->remove_text("Select...");
	click->append_text(get_button_text(last_click));
	click->append_text("Select...");
	click->set_active(2);
	set_by_me = false;
	have_last_click = true;
}

void Prefs::on_click_changed() {
	if (set_by_me)
		return;
	int n = click->get_active_row_number();
	if (n == -1)
		return;
	if (n <= 1) {
		{
		Ref<ButtonInfo> ref(prefs().click);
		ref->special = n + 1;
		}
		write();
		return;
	}
	if (n == 2 && have_last_click) {
		{
		Ref<ButtonInfo> ref(prefs().click);
		*ref = last_click;
		}
		write();
		return;
	}

	SelectButton sb(parent->widgets, prefs().click.get());
	if (sb.run()) {
		{
		Ref<ButtonInfo> ref(prefs().click);
		ref->button = sb.event.button;
		ref->state = sb.event.state;
		ref->special = 0;
		}
		write();
	}
	show_click();
}

void Prefs::show_stroke_click() {
	Ref<ButtonInfo> ref(prefs().stroke_click);
	if (ref->special) {
		set_by_me = true;
		stroke_click->set_active(ref->special-1);
		set_by_me = false;
		return;
	}
	if (ref->state == 0 && ref->button <= 3) {
		set_by_me = true;
		stroke_click->set_active(1+ref->button);
		set_by_me = false;
		return;
	}
	if (ref->state == 0 && (ref->button == 8 || ref->button == 9)) {
		set_by_me = true;
		stroke_click->set_active(ref->button-3);
		set_by_me = false;
		return;
	}
	stroke_click->set_title(get_button_text(*ref));
	set_by_me = true;
	if (have_last_stroke_click) {
		stroke_click->remove_text(get_button_text(last_stroke_click));
	}
	last_stroke_click = *ref;
	stroke_click->remove_text("Select...");
	stroke_click->append_text(get_button_text(last_stroke_click));
	stroke_click->append_text("Select...");
	stroke_click->set_active(7);
	set_by_me = false;
	have_last_stroke_click = true;
}

void Prefs::on_stroke_click_changed() {
	if (set_by_me)
		return;
	int n = stroke_click->get_active_row_number();
	if (n == -1)
		return;
	if (n <= 1) {
		{
		Ref<ButtonInfo> ref(prefs().stroke_click);
		ref->special = n + 1;
		}
		write();
		return;
	}
	if (n <= 4) {
		{
		Ref<ButtonInfo> ref(prefs().stroke_click);
		ref->state = 0;
		ref->button = n-1;
		ref->special = 0;
		}
		write();
		return;
	}
	if (n == 5 || n == 6) {
		{
		Ref<ButtonInfo> ref(prefs().stroke_click);
		ref->state = 0;
		ref->button = n+3;
		ref->special = 0;
		}
		write();
		return;
	}
	if (n == 7 && have_last_stroke_click) {
		{
		Ref<ButtonInfo> ref(prefs().stroke_click);
		*ref = last_stroke_click;
		}
		write();
		return;
	}

	SelectButton sb(parent->widgets, prefs().stroke_click.get());
	if (sb.run()) {
		{
		Ref<ButtonInfo> ref(prefs().stroke_click);
		ref->button = sb.event.button;
		ref->state = sb.event.state;
		ref->special = 0;
		}
		write();
	}
	show_stroke_click();
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

void Prefs::on_delay_changed() {
	prefs().delay.set(spin_delay->get_value());
	write();
}

void Prefs::on_delay_default() {
	spin_delay->set_value(pref_delay_default);
}

void Prefs::on_p_changed() {
	prefs().p.set(scale_p->get_value());
	write();
}

void Prefs::on_p_default() {
	scale_p->set_value(pref_p_default);
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



Glib::ustring get_button_text(ButtonInfo &bi) {
	struct {
		const guint mask;
		const char *name;
	} modnames[] = {
		{ShiftMask, "Shift"},
		{LockMask, "Caps"},
		{ControlMask, "Control"},
		{Mod1Mask, "Mod1"},
		{Mod2Mask, "Mod2"},
		{Mod3Mask, "Mod3"},
		{Mod4Mask, "Mod4"},
		{Mod5Mask, "Mod5"}
	};
	int n_modnames = 8;
	char button[16];
	sprintf(button, "Button %d", bi.button);
	Glib::ustring str;
	for (int i = 0; i < n_modnames; i++)
		if (bi.state & modnames[i].mask) {
			str += modnames[i].name;
			str += " + ";
		}
	return str + button;
}

void Prefs::set_button_label() {

	Ref<ButtonInfo> ref(prefs().button);
	blabel->set_text(get_button_text(*ref));
}
