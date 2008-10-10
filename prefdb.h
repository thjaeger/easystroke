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
#ifndef __PREFDB_H__
#define __PREFDB_H__
#include <string>
#include <set>
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>

#include "var.h"

enum TraceType { TraceStandard, TraceShape, TraceNone, TraceAnnotate, TraceFire, TraceN };

#define TO_OFF 0
#define TO_CONSERVATIVE 1
#define TO_MEDIUM 2
#define TO_AGGRESSIVE 3
#define TO_FLICK 4
#define TO_CUSTOM 5

class ButtonInfo {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		ar & button;
		ar & state;
		if (version == 1) {
			int special;
			ar & special;
			return;
		}
	}
public:
	guint button;
	guint state;
	void press();
	Glib::ustring get_button_text() const;
};
BOOST_CLASS_VERSION(ButtonInfo, 2)

extern const double default_p;
extern const int default_radius;
extern const int default_pressure_threshold;

class PrefDB : public TimeoutWatcher {
	friend class boost::serialization::access;
	bool good_state;
	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		ar & exceptions.unsafe_ref();
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
	}
public:
	PrefDB();

	Source<std::set<std::string> > exceptions;
	Source<double> p;
	Source<ButtonInfo> button;
	Source<TraceType> trace;
	Source<bool> advanced_ignore;
	Source<int> radius;
	Source<bool> ignore_grab;
	Source<bool> timing_workaround;
	Source<bool> show_clicks;
	Source<bool> pressure_abort;
	Source<int> pressure_threshold;
	Source<bool> proximity;
	Source<bool> feedback;
	Source<bool> left_handed;
	Source<int> init_timeout;
	Source<int> min_speed;
	Source<int> timeout_profile;
	Source<bool> timeout_gestures;
	Source<bool> tray_icon;

	void init();
	virtual void timeout();
};

BOOST_CLASS_VERSION(PrefDB, 9)

extern PrefDB prefs;
#endif
