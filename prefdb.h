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
#include <map>
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>
#include <gdkmm/color.h>

#include "var.h"

enum TraceType { TraceDefault, TraceUnused1, TraceNone, TraceAnnotate, TraceFire, TraceWater, TraceN };


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

typedef boost::shared_ptr<ButtonInfo> RButtonInfo;

struct RGBA {
	Gdk::Color color;
	guint16 alpha;
	RGBA() : alpha(65535) {}
	RGBA(Gdk::Color c) : color(c), alpha(65535) {}
	template<class Archive> void save(Archive &ar, unsigned int version) const {
		gushort r, g, b;
		r = color.get_red();
		g = color.get_green();
		b = color.get_blue();
		ar & r;
		ar & g;
		ar & b;
		ar & alpha;
	}
	template<class Archive> void load(Archive &ar, unsigned int version) {
		gushort r, g, b;
		ar & r;
		ar & g;
		ar & b;
		ar & alpha;
		color.set_red(r);
		color.set_green(g);
		color.set_blue(b);
	}
	bool operator==(const RGBA rgba) {
		return color == rgba.color && alpha == rgba.alpha;
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()
};

extern const double default_p;
extern const ButtonInfo default_button;
extern const int default_radius;
extern const int default_pressure_threshold;

class PrefDB : public TimeoutWatcher {
	friend class boost::serialization::access;
	bool good_state;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
public:
	PrefDB();

	Source<std::map<std::string, RButtonInfo> > exceptions;
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
	Source<std::set<std::string> > excluded_devices;
	Source<RGBA> color;
	Source<int> trace_width;

	void init();
	virtual void timeout();
};

BOOST_CLASS_VERSION(PrefDB, 12)

extern PrefDB prefs;
#endif
