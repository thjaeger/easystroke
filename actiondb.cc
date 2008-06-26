#include "actiondb.h"
#include "main.h"

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
BOOST_CLASS_EXPORT(SendKey)
BOOST_CLASS_EXPORT(Scroll)
BOOST_CLASS_EXPORT(Ignore)
BOOST_CLASS_EXPORT(Button)

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

template<class Archive> void SendKey::serialize(Archive & ar, const unsigned int version) {
	ar & boost::serialization::base_object<ModAction>(*this);
	ar & key;
	ar & code;
	ar & xtest;
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
	system(cmd.c_str());
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

ActionDB::ActionDB() : filename(config_dir+"actions"), current_id(0) {}

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

void ActionDB::read() {
	try {
		ifstream ifs(filename.c_str(), ios::binary);
		boost::archive::text_iarchive ia(ifs);
		ia >> *this;
		if (verbosity >= 2)
			cout << "Loaded " << strokes.size() << " actions." << endl;
	} catch (...) {
		cout << "Error: Couldn't read action database." << endl;
	}
}

bool ActionDB::write() const {
	try {
		ofstream ofs(filename.c_str());
		boost::archive::text_oarchive oa(ofs);
		const ActionDB db = *this;
		oa << db;
		if (verbosity >= 2)
			cout << "Saved " << strokes.size() << " actions." << endl;
		return true;
	} catch (...) {
		cout << "Error: Couldn't save action database." << endl;
		return false;
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

Ranking ActionDB::handle(RStroke s) {
	Ranking r;
	r.stroke = s;
	r.score = -1;
	bool success = false;
	if (!s)
		return r;
	for (StrokeIterator i = strokes_begin(); i; i++) {
		if (!i.stroke())
			continue;
		double score = Stroke::compare(s, i.stroke());
		if (score < -1.5)
			continue;
		r.r.insert(pair<double, pair<std::string, RStroke> >(score, pair<std::string, RStroke>(strokes[i.id()].name, i.stroke())));
		if (score >= r.score) {
			r.score = score;
			if (score >= 0.7) {
				r.id = i.id();
				r.name = strokes[r.id].name;
				r.action = i.action();
				success = true;
			}
		}
	}
	if (!success && s->trivial()) {
		r.id = -1;
		r.name = "button press (default)";
		success = true;
	}
	if (success) {
		if (verbosity >= 1)
			cout << "Excecuting Action " << r.name << "..." << endl;
		if (r.action)
			r.action->run();
	} else {
		if (verbosity >= 1)
			cout << "Couldn't find matching stroke." << endl;
	}
	return r;
}

LActionDB& actions() {
	static LActionDB actions_;
	return actions_;
}
