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
#include <glibmm/i18n.h>

#include <fstream>
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/export.hpp>

#include <X11/Xlib.h>

const double default_p = 0.5;
const ButtonInfo default_button(Button2);
const int default_radius = 16;
const int default_pressure_threshold = 192;

PrefDB::PrefDB() :
	TimeoutWatcher(5000),
	good_state(true),
	p(default_p),
	button(default_button),
	trace(TraceDefault),
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
	tray_icon(true),
	color(Gdk::Color("980101")),
	trace_width(5)
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
	if (trace.get() == TraceShape)
		trace.unsafe_ref() = TraceDefault;
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
	if (version < 12) {
		unsigned long c = 0;
		ar & c;
		color.unsafe_ref().color.set_rgb(257*(c >> 16), 257*((c >> 8) % 256), 257*(c % 256));
		return;
	} else {
		ar & color.unsafe_ref();
	}
	ar & trace_width.unsafe_ref();
	if (version < 13) return;
	ar & extra_buttons.unsafe_ref();
}

void PrefDB::timeout() {
	std::string filename = config_dir+"preferences";
	std::string tmp = filename + ".tmp";
	try {
		std::ofstream ofs(tmp.c_str());
		boost::archive::text_oarchive oa(ofs);
		const PrefDB *me = this;
		oa << *me;
		if (rename(tmp.c_str(), filename.c_str()))
			throw std::runtime_error("rename() failed");
		if (verbosity >= 2)
			printf("Saved preferences.\n");
	} catch (std::exception &e) {
		printf(_("Error: Couldn't save preferences: %s.\n"), e.what());
		if (!good_state)
			return;
		good_state = false;
		new ErrorDialog(
				_("Couldn't save preferences.  Your changes will be lost.  \nMake sure that ")+config_dir+
				_(" is a directory and that you have write access to it.\nYou can change the configuration directory using the -c or --config-dir command line options."));
	}
}


bool ButtonInfo::overlap(ButtonInfo &bi) const {
	if (button != bi.button)
		return false;
	if (state == AnyModifier || bi.state == AnyModifier)
		return true;
	return !((state ^ bi.state) & ~LockMask & ~Mod2Mask);
}

class TimeoutProfile : private Base {
	Out<int> &in;
public:
	virtual void notify() {
		switch (in.get()) {
			case TO_OFF:
				prefs.init_timeout.set(0);
				prefs.min_speed.set(0);
				break;
			case TO_CONSERVATIVE:
				prefs.init_timeout.set(240);
				prefs.min_speed.set(60);
				break;
			case TO_MEDIUM:
				prefs.init_timeout.set(30);
				prefs.min_speed.set(80);
				break;
			case TO_AGGRESSIVE:
				prefs.init_timeout.set(15);
				prefs.min_speed.set(150);
				break;
			case TO_FLICK:
				prefs.init_timeout.set(20);
				prefs.min_speed.set(500);
				break;
		}
	}
	TimeoutProfile(Out<int> &in_) : in(in_) {
		in.connect(this);
		notify();
	}
};

void PrefDB::init() {
	std::string filename = config_dir+"preferences";
	try {
		std::ifstream ifs(filename.c_str(), std::ios::binary);
		if (!ifs.fail()) {
			boost::archive::text_iarchive ia(ifs);
			ia >> *this;
			if (verbosity >= 2)
				std::cout << "Loaded preferences." << std::endl;
		}
	} catch (...) {
		printf(_("Error: Couldn't read preferences.\n"));
	}
	new TimeoutProfile(prefs.timeout_profile);
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
	watch(feedback);
	watch(left_handed);
	watch(init_timeout);
	watch(min_speed);
	watch(timeout_profile);
	watch(timeout_gestures);
	watch(tray_icon);
	watch(excluded_devices);
	watch(color);
	watch(trace_width);
	watch(extra_buttons);
}

PrefDB prefs;
