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
#ifndef __STROKE_H__
#define __STROKE_H__

#include <gdkmm/pixbuf.h>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>

#include <X11/X.h> // Time

#define STROKE_SIZE 64

class Stroke;
class PreStroke;

typedef boost::shared_ptr<Stroke> RStroke;
typedef boost::shared_ptr<PreStroke> RPreStroke;

struct Integral {
	struct {
		struct {
			double x;
			double y;
		} s, i; // square or not
	} d, i; // derivative or not
};

template<class T> class EasyIterator {
public:
	virtual const T operator++(int) = 0;
	virtual operator bool() = 0;
};

int get_default_button();

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
public:
	struct Point {
		double x;
		double y;
		double time;
		template<class Archive> void serialize(Archive & ar, const unsigned int version) {
			ar & x; ar & y; ar & time;
		}
	};

private:
	class RefineIterator;
	class InterpolateIterator;
	class RIIterator;

	class StrokeIntegral;
	class DiffIntegral;
	static inline Integral integral(EasyIterator<Point>&);
	Integral integral() const;
	static Integral diff_integral(RStroke a, RStroke b);
	static void integral2(RStroke a, RStroke b, double &int_x, double &int_y, double &int_dx, double &int_dy);
	double length() const;
	int size() const { return points.size(); }

	Stroke(PreStroke &s, int trigger_, int button_, bool timeout_);

	std::vector<Point> points;

	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		ar & points;
		if (version == 0) return;
		ar & button;
		if (version >= 2)
			ar & trigger;
		if (version < 4 && trigger == get_default_button())
			trigger = 0;
		if (version < 3)
			return;
		ar & timeout;
	}

public:
	int trigger;
	int button;
	bool timeout;

	Stroke() : trigger(0), button(0), timeout(false) {}
	static RStroke create(PreStroke &s, int trigger_, int button_, bool timeout_) {
		return RStroke(new Stroke(s, trigger_, button_, timeout_));
	}
	bool show_icon();

	static RStroke trefoil();
	static bool compare(RStroke, RStroke, double &);

	void print() const;
	void normalize();
	bool trivial() const { return size() == 0 && button == 0; }
	bool is_timeout() const { return timeout; }
};
BOOST_CLASS_VERSION(Stroke, 3)

class PreStroke : public std::vector<RTriple> {
public:
	static RPreStroke create() { return RPreStroke(new PreStroke()); }
	void add(RTriple p) { push_back(p); }
	bool valid() const { return size() > 2; }
};
#endif
