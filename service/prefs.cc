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
