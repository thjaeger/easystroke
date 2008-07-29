#include "prefdb.h"
#include "main.h"

#include <fstream>
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/export.hpp>

#include <X11/Xlib.h>

const double default_p = 0.5;
const ButtonInfo default_button = { Button2, 0 };
const int default_radius = 12;
const int default_pressure_threshold = 192;

PrefDB::PrefDB() :
	filename(config_dir+"preferences"),
	p(default_p),
	button(default_button),
	trace(TraceShape),
	advanced_ignore(false),
	radius(default_radius),
	ignore_grab(false),
	timing_workaround(false),
	show_clicks(true),
	pressure_abort(false),
	pressure_threshold(default_pressure_threshold),
	proximity(false)
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

