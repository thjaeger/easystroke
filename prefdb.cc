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
const ButtonInfo pref_button_default = { Button2, 0, 0 };
const ButtonInfo pref_click_default = { 0, 0, SPECIAL_DEFAULT };
const ButtonInfo pref_stroke_click_default = { 0, 0, SPECIAL_DEFAULT };
const int pref_delay_default = 0;

PrefDB::PrefDB() :
	filename(config_dir+"preferences"),
	p(pref_p_default),
	button(pref_button_default),
	help(true),
	trace(TraceShape),
	delay(pref_delay_default),
	click(pref_click_default),
	stroke_click(pref_stroke_click_default)
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
