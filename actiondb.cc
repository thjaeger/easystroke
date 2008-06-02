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

template<class Archive> void Action::serialize(Archive & ar, const unsigned int version) {}

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

template<class Archive> void StrokeSet::serialize(Archive & ar, const unsigned int version) {
	ar & boost::serialization::base_object<std::set<RStroke> >(*this);
}

template<class Archive> void StrokeInfo::serialize(Archive & ar, const unsigned int version) {
	ar & strokes;
	ar & action;
}

using namespace std;

bool Command::run() {
	system(cmd.c_str());
	return true;
}

inline ostream& operator<<(ostream& output, const Action& c) {
	return c.show(output);
}

ActionDB::ActionDB() : filename(config_dir+"actions") {}

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

bool ActionDB::remove(const string& name) {
	if (!has(name))
		return false;
	strokes.erase(name);
	return true;
}

bool ActionDB::rename(const string& name, const string& name2) {
	if (!has(name))
		return false;
	if (has(name2))
		return false;
	strokes[name2] = strokes[name];
	strokes.erase(name);
	write();
	return true;
}

bool ActionDB::addCmd(RStroke stroke, const string& name, const string& cmd) {
	if (has(name))
		return false;
	strokes[name].strokes.clear();
	if (stroke)
		strokes[name].strokes.insert(stroke);
	strokes[name].action = Command::create(cmd);
	write();
	return true;
}

bool ActionDB::changeCmd(const string& name, const string& cmd) {
	RCommand c = boost::dynamic_pointer_cast<Command>(strokes[name].action);
	if (c && c->cmd == cmd)
		return true;
	if (c)
		c->cmd = cmd;
	else
		strokes[name].action = Command::create(cmd);
	write();
	return true;
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
		r.r.insert(pair<double, pair<string, RStroke> >(score, pair<string, RStroke>(i.name(), i.stroke())));
		if (score >= r.score) {
			r.score = score;
			if (score >= 0.7) {
				r.name = i.name();
				r.action = i.action();
				success = true;
			}
		}
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
