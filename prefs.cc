#include "prefs.h"
#include "prefdb.h"
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

Prefs::Prefs(Win *parent_) : good_state(true), parent(parent_), q(sigc::mem_fun(*this, &Prefs::on_selected)) {
	Gtk::Button *bbutton, *add_exception, *remove_exception, *button_default_p, *button_default_delay;
	parent->widgets->get_widget("button_add_exception", add_exception);
	parent->widgets->get_widget("button_button", bbutton);
	parent->widgets->get_widget("button_default_delay", button_default_delay);
	parent->widgets->get_widget("button_default_p", button_default_p);
	parent->widgets->get_widget("button_remove_exception", remove_exception);
	parent->widgets->get_widget("combo_click", click);
	parent->widgets->get_widget("combo_stroke_click", stroke_click);
	parent->widgets->get_widget("combo_trace", trace);
	parent->widgets->get_widget("label_button", blabel);
	parent->widgets->get_widget("treeview_exceptions", tv);
	parent->widgets->get_widget("scale_p", scale_p);
	parent->widgets->get_widget("spin_delay", spin_delay);

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

class SelectButton : public Gtk::Dialog {
public:
	SelectButton(Gtk::Window& parent) :
		Gtk::Dialog("Select Button", parent, true),
		event(0),
		hbox(false, 12),
		image(Gtk::Stock::DIALOG_INFO, Gtk::ICON_SIZE_DIALOG),
		label("Please press the button that you want to be used for gestures in the box below.  You can also hold down modifiers.")
	{
		get_vbox()->set_spacing(10);
		get_vbox()->pack_start(hbox);
		hbox.pack_start(image);
		label.set_line_wrap();
		label.set_size_request(256,-1);
		hbox.pack_start(label);
		get_vbox()->pack_start(events);
		events.set_events(Gdk::BUTTON_PRESS_MASK);
		Glib::RefPtr<Gdk::Pixbuf> pb = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB,true,8,384,256);
		pb->fill(0x808080ff);
		WIDGET(Gtk::Image, box, pb);
		events.add(box);
		events.signal_button_press_event().connect(sigc::mem_fun(*this, &SelectButton::on_button_press));
		add_button(Gtk::Stock::CANCEL,1);
		show_all_children();
	}
	bool run2() {
		send(P_SUSPEND_GRAB);
		int response = run();
		send(P_RESTORE_GRAB);
		return response == Gtk::RESPONSE_NONE;
	}
	GdkEventButton *event;
private:
	bool on_button_press(GdkEventButton *ev) {
		event_ = *ev;
		event = &event_;
		hide();
		return true;
	}
	GdkEventButton event_;
	Gtk::HBox hbox;
	Gtk::Image image;
	Gtk::Label label;
	Gtk::EventBox events;
};

void Prefs::on_select_button() {
	SelectButton sb(parent->get_window());
	if (!sb.run2())
		return;
	{
		Ref<ButtonInfo> ref(prefs().button);
		ref->button = sb.event->button;
		ref->state = sb.event->state;
	}
	send(P_REGRAB);
	set_button_label();
	write();
}

bool set_by_me = false; //TODO: Come up with something better

void Prefs::show_click() {
	Ref<ButtonInfo> ref(prefs().click);
	if ((int)ref->button <= 0) {
		set_by_me = true;
		click->set_active(-ref->button);
		set_by_me = false;
		return;
	}
	click->set_title(get_button_text(*ref));
}

void Prefs::on_click_changed() {
	if (set_by_me)
		return;
	int n = click->get_active_row_number();
	if (n <= 1) {
		Ref<ButtonInfo> ref(prefs().click);
		ref->state = 0;
		ref->button = -n;
		return;
	}

	SelectButton sb(parent->get_window());
	if (sb.run2()) {
		Ref<ButtonInfo> ref(prefs().click);
		ref->button = sb.event->button;
		ref->state = sb.event->state;
	}
	show_click();
}

void Prefs::show_stroke_click() {
	Ref<ButtonInfo> ref(prefs().stroke_click);
	if ((int)ref->button <= 0) {
		set_by_me = true;
		stroke_click->set_active(1+ref->button);
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
}

void Prefs::on_stroke_click_changed() {
	if (set_by_me)
		return;
	int n = stroke_click->get_active_row_number();
	if (n <= 4) {
		Ref<ButtonInfo> ref(prefs().stroke_click);
		ref->state = 0;
		ref->button = n-1;
		return;
	}
	if (n == 5 || n == 6) {
		Ref<ButtonInfo> ref(prefs().stroke_click);
		ref->state = 0;
		ref->button = n+3;
		return;
	}

	SelectButton sb(parent->get_window());
	if (sb.run2()) {
		Ref<ButtonInfo> ref(prefs().stroke_click);
		ref->button = sb.event->button;
		ref->state = sb.event->state;
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
