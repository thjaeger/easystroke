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
#include "actions.h"
#include "actiondb.h"
#include "win.h"
#include "main.h"
#include "prefdb.h"
#include <glibmm/i18n.h>
#include <X11/XKBlib.h>
#include "grabber.h"
#include "cellrenderertextish.h"

#include <functional>
#include <typeinfo>

enum Type { MISC };


extern boost::shared_ptr<sigc::slot<void, RStroke> > stroke_action;
Source<bool> recording(false);

const Glib::ustring SendKey::get_label() const {
	return Gtk::AccelGroup::get_label(key, mods);
}

const Glib::ustring ModAction::get_label() const {
	if (!mods)
		return _("No Modifiers");
	Glib::ustring label = Gtk::AccelGroup::get_label(0, mods);
	return label.substr(0,label.size()-1);
}

Glib::ustring ButtonInfo::get_button_text() const {
	Glib::ustring str;
	if (instant)
		str += _("(Instantly) ");
	if (click_hold)
		str += _("(Click & Hold) ");
	if (state == AnyModifier)
		str += Glib::ustring() + "(" + _("Any Modifier") + " +) ";
	else
		str += Gtk::AccelGroup::get_label(0, (Gdk::ModifierType)state);
	return str + Glib::ustring::compose(_("Button %1"), button);
}

const Glib::ustring Scroll::get_label() const {
	if (mods)
		return ModAction::get_label() + _(" + Scroll");
	else
		return _("Scroll");
}

const Glib::ustring Ignore::get_label() const {
	if (mods)
		return ModAction::get_label();
	else
		return _("Ignore");
}