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

const ButtonInfo default_button(Button2);

PrefDB::PrefDB() :
	TimeoutWatcher(5000),
	good_state(true),
	button(default_button),
	advanced_ignore(false),
	proximity(false),
	feedback(true),
	left_handed(false),
	init_timeout(250),
	final_timeout(250),
	timeout_profile(TimeoutDefault),
	timeout_gestures(false),
	tray_icon(false),
	color(Gdk::Color("#980101")),
	trace_width(3),
	advanced_popups(true),
	scroll_invert(true),
	scroll_speed(2.0),
	tray_feedback(false),
	show_osd(true),
	move_back(false),
	whitelist(false)
{}

template<class Archive> void PrefDB::serialize(Archive & ar, const unsigned int version) {
	if (version < 11) {
		std::set<std::string> old;
		ar & old;
		for (std::set<std::string>::iterator i = old.begin(); i != old.end(); i++)
			exceptions.unsafe_ref()[*i] = RButtonInfo();
	} else ar & exceptions.unsafe_ref();
	if (version < 14) {
		double p = 0.5;
		ar & p;
	}
	ar & button.unsafe_ref();
	if (version < 2) {
		bool help;
		ar & help;
	}
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
	int radius = 16;
	ar & radius;
	if (version < 4) return;
	bool ignore_grab = false;
	ar & ignore_grab;
	bool timing_workaround = false;
	ar & timing_workaround;
	bool show_clicks = false;
	ar & show_clicks;
	bool pressure_abort = false;
	ar & pressure_abort;
	int pressure_threshold = 192;
	ar & pressure_threshold;
	ar & proximity.unsafe_ref();
	if (version < 5) return;
	ar & feedback.unsafe_ref();
	ar & left_handed.unsafe_ref();
	ar & init_timeout.unsafe_ref();
	ar & final_timeout.unsafe_ref();
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
	ar & advanced_popups.unsafe_ref();
	ar & scroll_invert.unsafe_ref();
	ar & scroll_speed.unsafe_ref();
	ar & tray_feedback.unsafe_ref();
	ar & show_osd.unsafe_ref();
	if (version < 16) return;
	ar & move_back.unsafe_ref();
	if (version < 17) return;
	ar & device_timeout.unsafe_ref();
	if (version < 18) return;
	ar & whitelist.unsafe_ref();
}

void PrefDB::timeout() {
	std::string filename = config_dir+"preferences"+prefs_versions[0];
	std::string tmp = filename + ".tmp";
	try {
		std::ofstream ofs(tmp.c_str());
		boost::archive::text_oarchive oa(ofs);
		const PrefDB *me = this;
		oa << *me;
		ofs.close();
		if (rename(tmp.c_str(), filename.c_str()))
			throw std::runtime_error("rename() failed");
		if (verbosity >= 2)
			printf("Saved preferences.\n");
	} catch (std::exception &e) {
		printf(_("Error: Couldn't save preferences: %s.\n"), e.what());
		if (!good_state)
			return;
		good_state = false;
		error_dialog(Glib::ustring::compose(_( "Couldn't save %1.  Your changes will be lost.  "
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

void PrefDB::init() {
	std::string filename = config_dir+"preferences";
	for (const char **v = prefs_versions; *v; v++) {
		if (is_file(filename + *v)) {
			filename += *v;
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
			break;
		}
	}
	std::map<std::string, RButtonInfo>::iterator i = exceptions.unsafe_ref().find("(window manager frame)");
	if (i != exceptions.unsafe_ref().end()) {
		RButtonInfo bi = i->second;
		exceptions.unsafe_ref().erase(i);
		exceptions.unsafe_ref()[""] = bi;
	}
}

PrefDB prefs;
