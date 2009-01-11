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
#include "prefdb.h"
#include "main.h"
#include "win.h"
#include <glibmm/i18n.h>

#include <fstream>
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/export.hpp>

#include <X11/Xlib.h>

const double default_p = 0.5;
const ButtonInfo default_button(Button2);
const int default_radius = 16;
const int default_pressure_threshold = 192;

#define INIT_SOURCE(x) x(data, &Proto::Prefs::x, &Proto::Prefs::set_##x)

PrefDB::PrefDB() :
	TimeoutWatcher(5000),
	good_state(true),
	INIT_SOURCE(p),
	button(default_button),
	INIT_SOURCE(trace),
	INIT_SOURCE(advanced_ignore),
	INIT_SOURCE(radius),
	INIT_SOURCE(ignore_grab),
	INIT_SOURCE(timing_workaround),
	INIT_SOURCE(pressure_abort),
	INIT_SOURCE(pressure_threshold),
	INIT_SOURCE(proximity),
	INIT_SOURCE(popups),
	INIT_SOURCE(left_handed),
	INIT_SOURCE(init_timeout),
	INIT_SOURCE(min_speed),
	INIT_SOURCE(timeout_profile),
	INIT_SOURCE(timeout_gestures),
	INIT_SOURCE(tray_icon),
	color(Gdk::Color("#980101")),
	INIT_SOURCE(trace_width),
	INIT_SOURCE(advanced_popups)
{}

void PrefDB::timeout() {
	std::string filename = config_dir+"preferences"+versions[0];
	std::string tmp = filename + ".tmp";
	try {
		if (verbosity >= 2)
			printf("Saved preferences.\n");
	} catch (std::exception &e) {
		printf(_("Error: Couldn't save preferences: %s.\n"), e.what());
		if (!good_state)
			return;
		good_state = false;
		new ErrorDialog(Glib::ustring::compose(_( "Couldn't save %1.  Your changes will be lost.  "
				"Make sure that \"%2\" is a directory and that you have write access to it.  "
				"You can change the configuration directory "
				"using the -c or --config-dir command line options."), _("preferences"), config_dir));
	}
}


bool ButtonInfo::overlap(const ButtonInfo &bi) const {
	if (button != bi.button)
		return false;
	if (state == AnyModifier || bi.state == AnyModifier)
		return true;
	return !((state ^ bi.state) & ~LockMask & ~Mod2Mask);
}

class TimeoutProfile : private Base {
	Out<TimeoutType> &in;
public:
	virtual void notify() {
		switch (in.get()) {
			case Proto::Prefs::Off:
				prefs.init_timeout.set(0);
				prefs.min_speed.set(0);
				break;
			case Proto::Prefs::Conservative:
				prefs.init_timeout.set(240);
				prefs.min_speed.set(60);
				break;
			case Proto::Prefs::Medium:
				prefs.init_timeout.set(30);
				prefs.min_speed.set(80);
				break;
			case Proto::Prefs::Aggressive:
				prefs.init_timeout.set(15);
				prefs.min_speed.set(150);
				break;
			case Proto::Prefs::Flick:
				prefs.init_timeout.set(20);
				prefs.min_speed.set(500);
				break;
			case Proto::Prefs::Custom:
				break;
		}
	}
	TimeoutProfile(Out<TimeoutType> &in_) : in(in_) {
		in.connect(this);
		notify();
	}
};

void PrefDB::init() {
	std::string filename = config_dir+"preferences";
	for (const char **v = versions; *v; v++) {
		if (is_file(filename + *v)) {
			filename += *v;
			try {
				std::ifstream ifs(filename.c_str(), std::ios::binary);
				if (!ifs.fail()) {
					boost::archive::text_iarchive ia(ifs);
//					ia >> *this;
					if (verbosity >= 2)
						std::cout << "Loaded preferences." << std::endl;
				}
			} catch (...) {
				printf(_("Error: Couldn't read preferences.\n"));
			}
			break;
		}
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
	watch(pressure_abort);
	watch(pressure_threshold);
	watch(proximity);
	watch(popups);
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
	watch(advanced_popups);
}

PrefDB prefs;
