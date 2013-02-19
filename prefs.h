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
#ifndef __PREFS_H__
#define __PREFS_H__
#include "prefdb.h"

#include <gtkmm.h>

class Prefs {
public:
	Prefs();
	virtual ~Prefs() {}
	void update_device_list();
	void update_extra_buttons();
private:
	void set_button_label();

	void on_add();
	void on_remove();
	void on_add_extra();
	void on_edit_extra();
	void on_remove_extra();
	void on_select_button();
public:
	void on_button_editing_started(GtkCellEditable *editable, const gchar *path);
private:
	void on_device_toggled(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter);
	void on_device_timeout_changed(const Glib::ustring& path, const Glib::ustring& new_text);
	bool select_row(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter, std::string name);

	class ExceptionColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		ExceptionColumns() { add(app); add(user_app); add(button); }
		Gtk::TreeModelColumn<std::string> app;
		Gtk::TreeModelColumn<Glib::ustring> user_app, button;
	};
	ExceptionColumns cols;
	Glib::RefPtr<Gtk::ListStore> tm;
	Gtk::TreeView* tv;

	class Single : public Gtk::TreeModel::ColumnRecord {
	public:
		Single() { add(name); }
		Gtk::TreeModelColumn<Glib::ustring> name;
	};
	Single timeout_columns;

	class DeviceColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		DeviceColumns() { add(enabled); add(name); add(timeout); }
		Gtk::TreeModelColumn<bool> enabled;
		Gtk::TreeModelColumn<Glib::ustring> name;
		Gtk::TreeModelColumn<Glib::ustring> timeout;
	};
	DeviceColumns dcs;
	Gtk::TreeView* dtv;
	Glib::RefPtr<Gtk::ListStore> dtm;

	class ExtraColumns : public Gtk::TreeModel::ColumnRecord {
	public:
		ExtraColumns() { add(str); add(i); }
		Gtk::TreeModelColumn<Glib::ustring> str;
		Gtk::TreeModelColumn<std::vector<ButtonInfo>::iterator> i;
	};
	ExtraColumns ecs;
	Gtk::TreeView *etv;
	Glib::RefPtr<Gtk::ListStore> etm;

	Gtk::SpinButton *spin_radius;
	Gtk::Label* blabel;
	Gtk::Frame *frame_tablet;
	bool ignore_device_toggled;
};

#endif
