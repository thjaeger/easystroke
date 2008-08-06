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

enum TraceType { TraceStandard, TraceShape, TraceNone };
const int trace_n = 3;

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
		ar & exceptions.write_ref(a);
		ar & p.write_ref(a);
		ar & button.write_ref(a);
		if (version < 2) {
			bool help;
			ar & help;
		}
		ar & trace.write_ref(a);
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
		ar & advanced_ignore.write_ref(a);
		ar & radius.write_ref(a);
		if (version < 4) return;
		ar & ignore_grab.write_ref(a);
		ar & timing_workaround.write_ref(a);
		ar & show_clicks.write_ref(a);
		ar & pressure_abort.write_ref(a);
		ar & pressure_threshold.write_ref(a);
		ar & proximity.write_ref(a);
		if (version < 5) return;
		ar & feedback.write_ref(a);
		ar & left_handed.write_ref(a);
	}
public:
	PrefDB();

	VarI<std::set<std::string> > exceptions;
	VarI<double> p;
	VarI<ButtonInfo> button;
	VarI<TraceType> trace;
	VarI<bool> advanced_ignore;
	VarI<int> radius;
	VarI<bool> ignore_grab;
	VarI<bool> timing_workaround;
	VarI<bool> show_clicks;
	VarI<bool> pressure_abort;
	VarI<int> pressure_threshold;
	VarI<bool> proximity;
	VarI<bool> feedback;
	VarI<bool> left_handed;

	void init();
	virtual void timeout();
};

BOOST_CLASS_VERSION(PrefDB, 5)

extern PrefDB prefs;
#endif
