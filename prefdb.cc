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
#include "prefdb.h"
#include "main.h"
#include "win.h"

#include <fstream>
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>
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
	proximity(false),
	feedback(true),
	left_handed(false),
	init_timeout(200),
	min_speed(40),
	timeout_profile(TO_CONSERVATIVE),
	timeout_gestures(false),
	tray_icon(true)
{}

template<class Archive> void PrefDB::serialize(Archive & ar, const unsigned int version) {
	if (version < 11) {
		std::set<std::string> old;
		ar & old;
		for (std::set<std::string>::iterator i = old.begin(); i != old.end(); i++)
			exceptions.unsafe_ref()[*i] = RButtonInfo();
	} else ar & exceptions.unsafe_ref();
	ar & p.unsafe_ref();
	ar & button.unsafe_ref();
	if (version < 2) {
		bool help;
		ar & help;
	}
	ar & trace.unsafe_ref();
	if (version < 3) {
		int delay;
		ar & delay;
	}
	if (version == 1) {
		ButtonInfo foo;
		ar & foo;
		ar & foo;
		return;
	}
	if (version < 2) return;
	if (version != 6)
		ar & advanced_ignore.unsafe_ref();
	ar & radius.unsafe_ref();
	if (version < 4) return;
	ar & ignore_grab.unsafe_ref();
	ar & timing_workaround.unsafe_ref();
	ar & show_clicks.unsafe_ref();
	ar & pressure_abort.unsafe_ref();
	ar & pressure_threshold.unsafe_ref();
	ar & proximity.unsafe_ref();
	if (version < 5) return;
	ar & feedback.unsafe_ref();
	ar & left_handed.unsafe_ref();
	ar & init_timeout.unsafe_ref();
	ar & min_speed.unsafe_ref();
	if (version < 8) return;
	ar & timeout_profile.unsafe_ref();
	if (version < 9) return;
	ar & timeout_gestures.unsafe_ref();
	ar & tray_icon.unsafe_ref();
	if (version < 10) return;
	ar & excluded_devices.unsafe_ref();
}

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
	watchValue(exceptions);
	watchValue(p);
	watchValue(button);
	watchValue(trace);
	watchValue(advanced_ignore);
	watchValue(radius);
	watchValue(ignore_grab);
	watchValue(timing_workaround);
	watchValue(show_clicks);
	watchValue(pressure_abort);
	watchValue(pressure_threshold);
	watchValue(proximity);
	watchValue(feedback);
	watchValue(left_handed);
	watchValue(init_timeout);
	watchValue(min_speed);
	watchValue(timeout_profile);
	watchValue(timeout_gestures);
	watchValue(tray_icon);
	watchValue(excluded_devices);
}

PrefDB prefs;
