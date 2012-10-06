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

enum TraceType { TraceDefault, TraceShape, TraceNone, TraceAnnotate, TraceFire, TraceWater };
enum TimeoutType { TimeoutOff, TimeoutDefault, TimeoutMedium, TimeoutAggressive, TimeoutFlick, TimeoutCustom, TimeoutConservative };

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
		if (version < 3)
			return;
		ar & instant;
		if (version < 4)
			return;
		ar & click_hold;
	}
public:
	guint button;
	guint state;
	bool instant;
	bool click_hold;
	bool operator<(const ButtonInfo &bi) const { return button < bi.button; }
	bool operator==(const ButtonInfo &bi) const {
		return button == bi.button && state == bi.state && !instant == !bi.instant && !click_hold == !bi.click_hold;
	}
	void press();
	Glib::ustring get_button_text() const;
	bool overlap(const ButtonInfo &bi) const;
	ButtonInfo(guint button_) : button(button_), state(0), instant(false), click_hold(false) {}
	ButtonInfo() : button(0), state(0), instant(false), click_hold(false) {}
};
BOOST_CLASS_VERSION(ButtonInfo, 4)

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

extern const ButtonInfo default_button;

class PrefDB : public TimeoutWatcher {
	friend class boost::serialization::access;
	bool good_state;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);

	template <class T> struct PrefSource : public Source<T> {
		PrefSource();
		PrefSource(T x_);
	};
public:
	PrefDB();

	PrefSource<std::map<std::string, RButtonInfo> > exceptions;
	PrefSource<ButtonInfo> button;
	PrefSource<TraceType> trace;
	PrefSource<bool> advanced_ignore;
	PrefSource<bool> proximity;
	PrefSource<bool> feedback;
	PrefSource<bool> left_handed;
	PrefSource<int> init_timeout;
	PrefSource<int> final_timeout;
	PrefSource<TimeoutType> timeout_profile;
	PrefSource<bool> timeout_gestures;
	PrefSource<bool> tray_icon;
	PrefSource<std::set<std::string> > excluded_devices;
	PrefSource<RGBA> color;
	PrefSource<int> trace_width;
	PrefSource<std::vector<ButtonInfo> > extra_buttons;
	PrefSource<bool> advanced_popups;
	PrefSource<bool> scroll_invert;
	PrefSource<double> scroll_speed;
	PrefSource<bool> tray_feedback;
	PrefSource<bool> show_osd;
	PrefSource<bool> move_back;
	PrefSource<std::map<std::string, TimeoutType> > device_timeout;
	PrefSource<bool> whitelist;
	PrefSource<bool> touch;

	void init();
	virtual void timeout();
};

BOOST_CLASS_VERSION(PrefDB, 19)

extern PrefDB prefs;

template <class T> PrefDB::PrefSource<T>::PrefSource() : Source<T>() { prefs.watch(*this); }
template <class T> PrefDB::PrefSource<T>::PrefSource(T x_) : Source<T>(x_) { prefs.watch(*this); }
#endif
