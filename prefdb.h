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

enum TraceType { TraceStandard = 0, TraceShape = 1, TraceNone = 2, TraceAnnotate = 3 };
const int trace_n = 4;

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
		Atomic a;
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
		if (version < 6) {
			bool advanced_ignore;
			ar & advanced_ignore;
		}
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
	}
public:
	PrefDB();

	VarI<std::set<std::string> > exceptions;
	VarI<double> p;
	VarI<ButtonInfo> button;
	VarI<TraceType> trace;
	VarI<int> radius;
	VarI<bool> ignore_grab;
	VarI<bool> timing_workaround;
	VarI<bool> show_clicks;
	VarI<bool> pressure_abort;
	VarI<int> pressure_threshold;
	VarI<bool> proximity;
	VarI<bool> feedback;
	VarI<bool> left_handed;
	VarI<int> init_timeout;
	VarI<int> min_speed;

	void init();
	virtual void timeout();
};

BOOST_CLASS_VERSION(PrefDB, 6)

extern PrefDB prefs;
#endif
