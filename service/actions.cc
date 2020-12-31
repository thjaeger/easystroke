#include "actions.h"
#include "actiondb.h"
#include "win.h"
#include "prefdb.h"
#include <glibmm/i18n.h>
#include <X11/XKBlib.h>

enum Type { MISC };


extern boost::shared_ptr<sigc::slot<void, RStroke> > stroke_action;
Source<bool> recording(false);

const Glib::ustring SendKey::get_label() const {
	return Gtk::AccelGroup::get_label(key, mods);
}

const Glib::ustring ModAction::get_label() const {
	if (!mods)
		return "No Modifiers";
	Glib::ustring label = Gtk::AccelGroup::get_label(0, mods);
	return label.substr(0,label.size()-1);
}

Glib::ustring ButtonInfo::get_button_text() const {
	Glib::ustring str;
	if (instant)
		str += "(Instantly) ";
	if (click_hold)
		str += "(Click & Hold) ";
	if (state == AnyModifier)
		str += Glib::ustring() + "(" + "Any Modifier" + " +) ";
	else
		str += Gtk::AccelGroup::get_label(0, (Gdk::ModifierType)state);
	return str + Glib::ustring::compose("Button %1", button);
}

const Glib::ustring Scroll::get_label() const {
	if (mods)
		return ModAction::get_label() + " + Scroll";
	else
		return "Scroll";
}

const Glib::ustring Ignore::get_label() const {
	if (mods)
		return ModAction::get_label();
	else
		return "Ignore";
}