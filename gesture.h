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
#ifndef __GESTURE_H__
#define __GESTURE_H__

#include "prefdb.h"
#include "stroke.h"
#include <gdkmm.h>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>

#include <X11/X.h>

#define STROKE_SIZE 64

class Stroke;
class PreStroke;

typedef boost::shared_ptr<Stroke> RStroke;
typedef boost::shared_ptr<PreStroke> RPreStroke;

struct Triple {
	float x;
	float y;
	Time t;
};
typedef boost::shared_ptr<Triple> RTriple;
void update_triple(RTriple e, float x, float y, Time t);
RTriple create_triple(float x, float y, Time t);

class PreStroke;
class Stroke {
	friend class PreStroke;
	friend class boost::serialization::access;
	friend class Stats;
public:
	struct Point {
		double x;
		double y;
		Point operator+(const Point &p) {
			Point sum = { x + p.x, y + p.y };
			return sum;
		}
		Point operator-(const Point &p) {
			Point sum = { x - p.x, y - p.y };
			return sum;
		}
		Point operator*(const double a) {
			Point product = { x * a, y * a };
			return product;
		}
		template<class Archive> void serialize(Archive & ar, const unsigned int version) {
			ar & x; ar & y;
			if (version == 0) {
				double time;
				ar & time;
			}
		}
	};

private:
	Stroke(PreStroke &s, int trigger_, int button_, unsigned int modifiers_, bool timeout_);

	Glib::RefPtr<Gdk::Pixbuf> draw_(int size, double width = 2.0, bool inv = false) const;
	mutable Glib::RefPtr<Gdk::Pixbuf> pb[2];

	static Glib::RefPtr<Gdk::Pixbuf> drawEmpty_(int);
	static Glib::RefPtr<Gdk::Pixbuf> pbEmpty;

	BOOST_SERIALIZATION_SPLIT_MEMBER()
	template<class Archive> void load(Archive & ar, const unsigned int version) {
		std::vector<Point> ps;
		ar & ps;
		if (ps.size()) {
			stroke_t *s = stroke_alloc(ps.size());
			for (std::vector<Point>::iterator i = ps.begin(); i != ps.end(); ++i)
				stroke_add_point(s, i->x, i->y);
			stroke_finish(s);
			stroke.reset(s, &stroke_free);
		}
		if (version == 0) return;
		ar & button;
		if (version >= 2)
			ar & trigger;
		if (version < 4 && (!button || trigger == (int)prefs.button.get().button))
			trigger = 0;
		if (version < 3)
			return;
		ar & timeout;
		if (version < 5)
			return;
		ar & modifiers;

	}
	template<class Archive> void save(Archive & ar, const unsigned int version) const {
		std::vector<Point> ps;
		for (unsigned int i = 0; i < size(); i++)
			ps.push_back(points(i));
		ar & ps;
		ar & button;
		ar & trigger;
		ar & timeout;
		ar & modifiers;
	}
public:
	int trigger;
	int button;
	unsigned int modifiers;
	bool timeout;
	boost::shared_ptr<stroke_t> stroke;

	Stroke() : trigger(0), button(0), modifiers(AnyModifier), timeout(false) {}
	static RStroke create(PreStroke &s, int trigger_, int button_, unsigned int modifiers_, bool timeout_) {
		return RStroke(new Stroke(s, trigger_, button_, modifiers_, timeout_));
	}
        Glib::RefPtr<Gdk::Pixbuf> draw(int size, double width = 2.0, bool inv = false) const;
	void draw(Cairo::RefPtr<Cairo::Surface> surface, int x, int y, int w, int h, double width = 2.0, bool inv = false) const;
	void draw_svg(std::string filename) const;
	bool show_icon();

	static RStroke trefoil();
	static int compare(RStroke, RStroke, double &);
	static Glib::RefPtr<Gdk::Pixbuf> drawEmpty(int);
	static Glib::RefPtr<Gdk::Pixbuf> drawDebug(RStroke, RStroke, int);

	unsigned int size() const { return stroke ? stroke_get_size(stroke.get()) : 0; }
	bool trivial() const { return size() == 0 && button == 0; }
	Point points(int n) const { Point p; stroke_get_point(stroke.get(), n, &p.x, &p.y); return p; }
	double time(int n) const { return stroke_get_time(stroke.get(), n); }
	bool is_timeout() const { return timeout; }
};
BOOST_CLASS_VERSION(Stroke, 5)
BOOST_CLASS_VERSION(Stroke::Point, 1)

class PreStroke : public std::vector<RTriple> {
public:
	static RPreStroke create() { return RPreStroke(new PreStroke()); }
	void add(RTriple p) { push_back(p); }
	bool valid() const { return size() > 2; }
};
#endif
