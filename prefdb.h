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

#include "proto.pb.h"
#include "var.h"

typedef Proto::Prefs::Trace TraceType;
typedef Proto::Prefs::Timeout TimeoutType;

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
	}
public:
	guint button;
	guint state;
	bool instant;
	bool operator<(const ButtonInfo &bi) const { return button < bi.button; }
	bool operator==(const ButtonInfo &bi) const {
		return button == bi.button && state == bi.state && !instant == !bi.instant;
	}
	void press();
	Glib::ustring get_button_text() const;
	bool overlap(const ButtonInfo &bi) const;
	ButtonInfo(guint button_) : button(button_), state(0), instant(false) {}
	ButtonInfo() : button(0), state(0), instant(false) {}
};
BOOST_CLASS_VERSION(ButtonInfo, 3)

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

template <class T> class DataSource : public IO<T> {
	typedef T (Proto::Prefs::*Getter)() const;
	typedef void (Proto::Prefs::*Setter)(T);
	Proto::Prefs &data;
	Getter getter;
	Setter setter;
public:
	DataSource(Proto::Prefs &data_, Getter getter_, Setter setter_) : data(data_), getter(getter_), setter(setter_) {}
	virtual void set(const T x) {
		(data.*setter)(x);
		Out<T>::update();
	}
	virtual T get() const { return (data.*getter)(); }
};


extern const double default_p;
extern const ButtonInfo default_button;
extern const int default_radius;
extern const int default_pressure_threshold;

class PrefDB : public TimeoutWatcher {
	friend class boost::serialization::access;
	bool good_state;

	Proto::Prefs data;
public:
	PrefDB();

	Source<std::map<std::string, RButtonInfo> > exceptions;
	DataSource<float> p;
	Source<ButtonInfo> button;
	DataSource<TraceType> trace;
	DataSource<bool> advanced_ignore;
	DataSource<int32_t> radius;
	DataSource<bool> ignore_grab;
	DataSource<bool> timing_workaround;
	DataSource<bool> pressure_abort;
	DataSource<int32_t> pressure_threshold;
	DataSource<bool> proximity;
	DataSource<bool> popups;
	DataSource<bool> left_handed;
	DataSource<int32_t> init_timeout;
	DataSource<int32_t> min_speed;
	DataSource<TimeoutType> timeout_profile;
	DataSource<bool> timeout_gestures;
	DataSource<bool> tray_icon;
	Source<std::set<std::string> > excluded_devices;
	Source<RGBA> color;
	DataSource<int32_t> trace_width;
	Source<std::vector<ButtonInfo> > extra_buttons;
	DataSource<bool> advanced_popups;

	void init();
	virtual void timeout();
};

BOOST_CLASS_VERSION(PrefDB, 13)

extern PrefDB prefs;
#endif
