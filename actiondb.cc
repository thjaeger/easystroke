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
#include "actiondb.h"
#include "main.h"
#include "win.h"

#include <iostream>
#include <fstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/shared_ptr.hpp>
//#include <boost/serialization/base_object.hpp>

// I don't know WHY I need this, I'm just glad I found out I did...
BOOST_CLASS_EXPORT(StrokeSet)

BOOST_CLASS_EXPORT(Action)
BOOST_CLASS_EXPORT(Command)
BOOST_CLASS_EXPORT(ModAction)
BOOST_CLASS_EXPORT(Scroll)
BOOST_CLASS_EXPORT(Ignore)
BOOST_CLASS_EXPORT(Button)

class SendKeyPriv : public SendKey {
	friend class boost::serialization::access;
	friend class SendKey;
	guint key;
	guint code;
	BOOST_SERIALIZATION_SPLIT_MEMBER()
	template<class Archive> void load(Archive & ar, const unsigned int version) {
		ar & boost::serialization::base_object<ModAction>(*this);
		ar & key;
		ar & code;
		if (version < 1) {
			bool xtest;
			ar & xtest;
		}
		compute_code();
	}
	template<class Archive> void save(Archive & ar, const unsigned int version) const {
		ar & boost::serialization::base_object<ModAction>(*this);
		ar & key;
		ar & code;
	}

	SendKeyPriv(guint key_, Gdk::ModifierType mods, guint code_) :
		SendKey(mods), key(key_), code(code_) { compute_code(); }

	void compute_code() {
		if (key)
			code = XKeysymToKeycode(dpy, key);
	}
public:
	SendKeyPriv() {}
	virtual bool run() {
		press();
		XTestFakeKeyEvent(dpy, code, true, 0);
		XTestFakeKeyEvent(dpy, code, false, 0);
		return true;
	}

	virtual const Glib::ustring get_label() const {
		Glib::ustring str = Gtk::AccelGroup::get_label(key, mods);
		if (key == 0) {
			char buf[10];
			snprintf(buf, 9, "0x%x", code);
			str += buf;
		}
		return str;
	}
};

BOOST_CLASS_VERSION(SendKeyPriv, 1)
BOOST_CLASS_EXPORT_GUID(SendKeyPriv, "SendKey")

RSendKey SendKey::create(guint key, Gdk::ModifierType mods, guint code) {
	return RSendKey(new SendKeyPriv(key, mods, code));
}

template<class Archive> void Action::serialize(Archive & ar, const unsigned int version) {
}

template<class Archive> void Command::serialize(Archive & ar, const unsigned int version) {
	ar & boost::serialization::base_object<Action>(*this);
	ar & cmd;
}

template<class Archive> void ModAction::serialize(Archive & ar, const unsigned int version) {
	ar & boost::serialization::base_object<Action>(*this);
	ar & mods;
}

template<class Archive> void Scroll::serialize(Archive & ar, const unsigned int version) {
	ar & boost::serialization::base_object<ModAction>(*this);
}

template<class Archive> void Ignore::serialize(Archive & ar, const unsigned int version) {
	ar & boost::serialization::base_object<ModAction>(*this);
}

template<class Archive> void Button::serialize(Archive & ar, const unsigned int version) {
	ar & boost::serialization::base_object<ModAction>(*this);
	ar & button;
}

template<class Archive> void StrokeSet::serialize(Archive & ar, const unsigned int version) {
	ar & boost::serialization::base_object<std::set<RStroke> >(*this);
}

template<class Archive> void StrokeInfo::serialize(Archive & ar, const unsigned int version) {
	ar & strokes;
	ar & action;
	if (version == 0) return;
	ar & name;
}

using namespace std;

bool Command::run() {
	if (cmd == "")
		return false;
	pid_t pid = fork();
	switch (pid) {
		case 0:
			execlp("/bin/sh", "sh", "-c", cmd.c_str(), NULL);
			exit(1);
		case -1:
			printf("Error: can't execute command %s: fork failed\n", cmd.c_str());
	}
	return true;
}

ButtonInfo Button::get_button_info() const {
	ButtonInfo bi;
	bi.button = button;
	bi.state = mods;
	return bi;
}


const Glib::ustring Button::get_label() const {
	return get_button_info().get_button_text();
}

ActionDB::ActionDB() : current_id(0) {}

template<class Archive> void ActionDB::load(Archive & ar, const unsigned int version) {
	if (version >= 1) {
		std::map<int, StrokeInfo> strokes2;
		ar & strokes2;
		for (std::map<int, StrokeInfo>::iterator i = strokes2.begin(); i != strokes2.end(); ++i)
			add(i->second);
		return;
	} else {
		std::map<std::string, StrokeInfo> strokes2;
		ar & strokes2;
		for (std::map<std::string, StrokeInfo>::iterator i = strokes2.begin(); i != strokes2.end(); ++i) {
			i->second.name = i->first;
			add(i->second);
		}
		return;
	}
}

void ActionDBWatcher::init() {
	std::string filename = config_dir+"actions";
	try {
		ifstream ifs(filename.c_str(), ios::binary);
		boost::archive::text_iarchive ia(ifs);
		ia >> actions.unsafe_ref();
		if (verbosity >= 2)
			cout << "Loaded " << actions.ref().strokes.size() << " actions." << endl;
	} catch (...) {
		cout << "Error: Couldn't read action database." << endl;
	}
	watch(actions);
}

void ActionDBWatcher::timeout() {
	std::string filename = config_dir+"actions";
	try {
		ofstream ofs(filename.c_str());
		boost::archive::text_oarchive oa(ofs);
		const ActionDB &db = actions.ref();
		oa << db;
		if (verbosity >= 2)
			cout << "Saved " << actions.ref().strokes.size() << " actions." << endl;
	} catch (...) {
		cout << "Error: Couldn't save action database." << endl;
		if (!good_state)
			return;
		good_state = false;
		new ErrorDialog("Couldn't save actions.  Your changes will be lost.  \nMake sure that "+config_dir+" is a directory and that you have write access to it.\nYou can change the configuration directory using the -c or --config-dir command line options.");
	}
}

bool ActionDB::remove(int id) {
	return strokes.erase(id);
}

int ActionDB::add(StrokeInfo &si) {
	strokes[current_id] = si;
	return current_id++;
}


int ActionDB::addCmd(RStroke stroke, const string& name, const string& cmd) {
	StrokeInfo si;
	if (stroke)
		si.strokes.insert(stroke);
	si.name = name;
	si.action = Command::create(cmd);
	return add(si);
}


int ActionDB::nested_size() const {
	int size = 0;
	for (const_iterator i = begin(); i != end(); i++)
		size += i->second.strokes.size();
	return size;
}

Ranking *ActionDB::handle(RStroke s, int button_up) const {
	Ranking *r = new Ranking;
	r->stroke = s;
	r->score = -1;
	r->id = -3;
	bool success = false;
	if (!s)
		return r;
	for (StrokeIterator i = strokes_begin(); i; i++) {
		if (!i.stroke())
			continue;
		double score;
	       	bool match = Stroke::compare(s, i.stroke(), score);
		if (score < 0.25)
			continue;
		r->r.insert(pair<double, pair<std::string, RStroke> >
				(score, pair<std::string, RStroke>(i.name(), i.stroke())));
		if (score >= r->score) {
			r->score = score;
			if (match) {
				r->id = i.id();
				r->name = i.name();
				r->action = i.action();
				r->best_stroke = i.stroke();
				success = true;
			}
		}
	}
	if (!success && s->trivial()) {
		r->id = s->is_timeout() ? -2 : -1;
		r->name = "click (default)";
		if (!s->is_timeout())
			success = true;
	}
	if (success) {
		if (verbosity >= 1)
			cout << "Excecuting Action " << r->name << "..." << endl;
		if (button_up)
			XTestFakeButtonEvent(dpy, button_up, False, CurrentTime);
		if (r->action)
			r->action->run();
	} else {
		if (verbosity >= 1)
			cout << "Couldn't find matching stroke." << endl;
	}
	return r;
}

Source<ActionDB> actions;
