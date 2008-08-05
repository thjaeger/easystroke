#include "prefdb.h"
#include "main.h"
#include "win.h" // TODO: We don't need this in the long run

#include <fstream>
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/export.hpp>

#include <X11/Xlib.h>

const double default_p = 0.5;
const ButtonInfo default_button = { Button2, 0 };
const int default_radius = 16;
const int default_pressure_threshold = 192;

PrefDB::PrefDB() :
	TimeoutWatcher(5000),
	good_state(true),
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

void PrefDB::timeout() {
	std::string filename = config_dir+"preferences";
	try {
		std::ofstream ofs(filename.c_str());
		boost::archive::text_oarchive oa(ofs);
		const PrefDB *me = this;
		oa << *me;
		if (verbosity >= 2)
			std::cout << "Saved preferences." << std::endl;
	} catch (...) {
		std::cout << "Error: Couldn't save preferences." << std::endl;
		if (!good_state)
			return;
		good_state = false;
		new ErrorDialog("Couldn't save preferences.  Your changes will be lost.  \nMake sure that "+config_dir+" is a directory and that you have write access to it.\nYou can change the configuration directory using the -c or --config-dir command line options.");
	}
}

void PrefDB::init() {
	std::string filename = config_dir+"preferences";
	try {
		std::ifstream ifs(filename.c_str(), std::ios::binary);
		boost::archive::text_iarchive ia(ifs);
		ia >> *this;
		if (verbosity >= 2)
			std::cout << "Loaded preferences." << std::endl;
	} catch (...) {
		std::cout << "Error: Couldn't read preferences." << std::endl;
	}
	watch(exceptions);
	watch(p);
	watch(button);
	watch(trace);
	watch(advanced_ignore);
	watch(radius);
	watch(ignore_grab);
	watch(timing_workaround);
	watch(show_clicks);
	watch(pressure_abort);
	watch(pressure_threshold);
	watch(proximity);
}

PrefDB prefs;
