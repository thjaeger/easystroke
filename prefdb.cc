#include "prefdb.h"
#include "main.h"

#include <fstream>
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/export.hpp>

#include <X11/Xlib.h>

const double pref_p_default = 0.5;
const ButtonInfo pref_button_default = { Button2, 0 };
const int pref_delay_default = 0;

PrefDB::PrefDB() :
	filename(config_dir+"preferences"),
	p(pref_p_default),
	button(pref_button_default),
	trace(TraceShape),
	delay(pref_delay_default),
	cds_stroke(false)
{}

bool PrefDB::write() const {
	try {
		std::ofstream ofs(filename.c_str());
		boost::archive::text_oarchive oa(ofs);
		const PrefDB *me = this;
		oa << *me;
		if (verbosity >= 2)
			std::cout << "Saved preferences." << std::endl;
		return true;
	} catch (...) {
		std::cout << "Error: Couldn't save preferences." << std::endl;
		return false;
	}
}

void PrefDB::read() {
	try {
		std::ifstream ifs(filename.c_str(), std::ios::binary);
		boost::archive::text_iarchive ia(ifs);
		ia >> *this;
		if (verbosity >= 2)
			std::cout << "Loaded preferences." << std::endl;
	} catch (...) {
		std::cout << "Error: Couldn't read preferences." << std::endl;
	}
}

PrefDB& prefs() {
	static PrefDB prefs_;
	return prefs_;
}

Glib::ustring ButtonInfo::get_button_text() {
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
	char name[16];
	sprintf(name, "Button %d", button);
	Glib::ustring str;
	for (int i = 0; i < n_modnames; i++)
		if (state & modnames[i].mask) {
			str += modnames[i].name;
			str += " + ";
		}
	return str + name;
}
