#include "action.h"
#include "win.h"
#include "prefdb.h"

extern std::shared_ptr<sigc::slot<void, RStroke> > stroke_action;

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
