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
#include "stroke.h"
#include "prefdb.h"
#include <math.h>
#include <iterator>
#include <functional>

#define eps 0.000001

using namespace std;

inline bool close(double x, double y) {
	double diff = x - y;
	if (diff < 0)
		diff = -diff;
	return diff < eps;
}

int get_default_button() { return prefs.button.get().button; }

inline double sqr(double x) { return x*x; };

void update_triple(RTriple e, float x, float y, Time t) {
	e->x = x;
	e->y = y;
	e->t = t;
}

RTriple create_triple(float x, float y, Time t) {
	RTriple e(new Triple);
	update_triple(e, x, y, t);
	return e;
}

struct f : public std::unary_function<RTriple, Stroke::Point> {
	Stroke::Point operator()(RTriple e) {
		Stroke::Point p = { e->x, e->y, e->t };
		return p;
	}
};

Stroke::Stroke(PreStroke &s, int trigger_, int button_) : trigger(trigger_), button(button_) {
	if (s.valid()) {
		std::transform(s.points.begin(), s.points.end(), std::back_inserter(points), f());
		normalize();
	}
}

void Stroke::normalize() {
	if (0) {
		double first = points.front().time;
		double length = points.back().time - first;
		for (vector<Point>::iterator i = points.begin(); i != points.end(); i++) {
			i->time -= first;
			i->time /= length;
		}
	} else {
		double total = 0;
		double lastx = 0;
		double lasty = 0;
		bool first = true;
		for (vector<Point>::iterator i = points.begin(); i != points.end(); i++) {
			if (first) {
				i->time = 0;
				lastx = i->x;
				lasty = i->y;
				first = false;
				continue;
			}
			total += hypot(i->x-lastx, i->y-lasty);
			i->time = total;
			lastx = i->x;
			lasty = i->y;
		}
		for (vector<Point>::iterator i = points.begin(); i != points.end(); i++) {
			i->time /= total;
		}
	}

	double minX=0, minY=0, maxX=0, maxY=0;
	bool first = true;
	for (vector<Point>::iterator i = points.begin(); i!=points.end();i++) {
		if (first) {
			minX = i->x;
			maxX = i->x;
			minY = i->y;
			maxY = i->y;
			first = false;
		} else {
			if (i->x < minX) minX = i->x;
			if (i->x > maxX) maxX = i->x;
			if (i->y < minY) minY = i->y;
			if (i->y > maxY) maxY = i->y;
		}
	}
	double scaleX = maxX - minX;
	double scaleY = maxY - minY;
	double scale = (scaleX > scaleY) ? scaleX : scaleY;
	if (scale < 0.001) scale = 1;
	for (vector<Point>::iterator i = points.begin(); i != points.end(); i++) {
	   i->x = (i->x-(minX+maxX)/2)/scale + 0.5;
	   i->y = (i->y-(minY+maxY)/2)/scale + 0.5;
	}
}

void Stroke::print() const {
	printf("button: %d\n", button);
	for (vector<Point>::const_iterator i = points.begin(); i != points.end(); i++) {
		printf("pt: (%f, %f) at %f\n", i->x, i->y, i->time);
	}
}

double Stroke::length() const {
	double length = 0;
	bool first = true;
	double lastx = 0;
	double lasty = 0;
	for (vector<Point>::const_iterator i = points.begin(); i != points.end(); i++) {
		if (first) {
			lastx = i->x;
			lasty = i->y;
			first = false;
			continue;
		}
		length += hypot(i->x-lastx, i->y-lasty);
		lastx = i->x;
		lasty = i->y;
	}
	return length;
}

/********* Iterators **********/
struct Pt {
	double x; double y;
};

struct PtPair {
	double t;
	Pt a;
	Pt b;
};

class Stroke::RefineIterator {
	vector<Point>::const_iterator i, i_end, j, j_end;
public:
	RefineIterator(RStroke a, RStroke b) :
		i(a->points.begin()), i_end(a->points.end()), j(b->points.begin()), j_end(b->points.end()) {}
	double operator++(int) {
		if (close(i->time, j->time)) {
			double current = (i->time + j->time)/2;
			i++; j++;
			return current;
		}
		if (i->time < j->time) {
			double current = i->time;
			i++;
			return current;
		} else {
			double current = j->time;
			j++;
			return current;
		}
	}
	operator bool() {
		return i != i_end && j != j_end;
	}
};

class Stroke::InterpolateIterator {
	RefineIterator &t;
	vector<Point>::const_iterator j, j_end;
	const Point *a, *b; // The current line segment goes from a to b
	Pt current;
public:
	InterpolateIterator(RefineIterator &t_, RStroke &in) :
		t(t_),
		j(in->points.begin()),
		j_end(in->points.end()),
		a(0), b(0)
	{}
	operator bool() { return t; }
	Pt operator++(int) {
		double now = t++;
		while (j != j_end && (!a || b->time < now)) {
			a = b;
			b = &(*j);
			j++;
		}
		double delta = b->time - a->time;
		if (delta < eps) {
			current.x = b->x;
			current.y = b->y;
		} else {
			double k = (now - a->time) / delta;
			current.x = a->x + (b->x - a->x) * k;
			current.y = a->y + (b->y - a->y) * k;
		}
		return current;
	}
};

class Stroke::RIIterator {
	// There is more potential for optimization here
	RefineIterator t;
	RefineIterator ti;
	RefineIterator tj;
	InterpolateIterator i, j;
	PtPair p;
public:
	RIIterator(RStroke a, RStroke b) :
		t(a, b),
		ti(a, b),
		tj(a, b),
		i(ti, a),
		j(tj, b)
	{}
	operator bool() { return t; }
	PtPair operator++(int) {
		p.t = t++;
		p.a = i++;
		p.b = j++;
		return p;
	}
};

class Stroke::DiffIntegral : public EasyIterator<Point> {
	RIIterator i;
	double a_length, b_length;
public:
	DiffIntegral(RStroke a, RStroke b) : i(a, b), a_length(a->length()), b_length(b->length()) {}
	inline virtual const Point operator++(int) {
		PtPair ps = i++;
		Point p;
		p.x = ps.a.x/a_length - ps.b.x/b_length;
		p.y = ps.a.y/a_length - ps.b.y/b_length;
		p.time = ps.t;
		return p;
	}
	inline virtual operator bool() { return i; }
};

class Stroke::StrokeIntegral : public EasyIterator<Point> {
	vector<Point>::const_iterator i, i_end;
public:
	StrokeIntegral(const Stroke& s) : i(s.points.begin()), i_end(s.points.end()) {}
	inline virtual const Point operator++(int) { return *(i++); }
	inline virtual operator bool() { return i != i_end; }
};

/******** (Iterators) *********/

Integral Stroke::diff_integral(RStroke a, RStroke b) {
	DiffIntegral di(a, b);
	return integral(di);
}

Integral Stroke::integral() const{
	StrokeIntegral si(*this);
	return integral(si);
}

Integral Stroke::integral(EasyIterator<Point>& i) {
	Integral sum = {{{0,0},{0,0}},{{0,0},{0,0}}};
	Point a;
	Point b = i++;
	while (i) {
		a = b;
		b = i++;
		double delta = b.time - a.time;
		if (delta < eps)
			continue;
#define INT_II(l, c) delta * (c + l) / 2
		sum.i.i.x += INT_II(a.x, b.x);
		sum.i.i.y += INT_II(a.y, b.y);
#undef INT_II
#define INT_IS(l, c) delta * (c*(c+l)+l*l) / 3
		sum.i.s.x += INT_IS(a.x, b.x);
		sum.i.s.y += INT_IS(a.y, b.y);
#undef INT_IS
		sum.d.i.x += b.x - a.x;
		sum.d.i.y += b.y - a.y;
#define INT_DS(l, c) sqr(c-l)/delta
		sum.d.s.x += INT_DS(a.x, b.x);
		sum.d.s.y += INT_DS(a.y, b.y);
#undef INT_DS
	}
	return sum;
}

void Stroke::integral2(RStroke a, RStroke b, double &int_x, double &int_y, double &int_dx, double &int_dy) {
	PtPair cur = { 0, {0,0}, {0,0} };
	PtPair last = { 0, {0,0}, {0,0} };
	int_x = 0; int_y = 0; int_dx = 0; int_dy = 0;

	for (RIIterator i(a, b); i;) {
		last = cur;
		cur  = i++;
		double delta = cur.t - last.t;
		int_x += delta*(2*cur.a.x*cur.b.x+2*last.a.x*last.b.x+cur.a.x*last.b.x+cur.b.x*last.a.x)/6;
		int_y += delta*(2*cur.a.y*cur.b.y+2*last.a.y*last.b.y+cur.a.y*last.b.y+cur.b.y*last.a.y)/6;
		if (delta < eps)
			continue;
		int_dx += (cur.a.x-last.a.x)*(cur.b.x-last.b.x)/delta;
		int_dy += (cur.a.y-last.a.y)*(cur.b.y-last.b.y)/delta;
	}
}

double Stroke::compare(RStroke a, RStroke b) {
	return compare(a, b, prefs.p.get());
}

double Stroke::compare(RStroke a_, RStroke b_, double p) {
	if (!a_ || !b_)
		return -2;
	if (a_->button != b_->button)
		if (!(a_->button == b_->trigger && b_->button == a_->trigger))
			return -2;
	if (a_->size() == 0 || b_->size() == 0) {
		if (a_->size() == 0 && b_->size() == 0)
			return 1;
		else
			return -2;
	}
	if (1) {
		double ab_x, ab_y, dab_x, dab_y;
		integral2(a_, b_, ab_x, ab_y, dab_x, dab_y);
		Integral a = a_->integral();
		Integral b = b_->integral();
		double A = (a.i.s.x - sqr(a.i.i.x)) + (a.i.s.y - sqr(a.i.i.y));
		double B = (b.i.s.x - sqr(b.i.i.x)) + (b.i.s.y - sqr(b.i.i.y));
		double C = (ab_x - a.i.i.x * b.i.i.x) + (ab_y - a.i.i.y * b.i.i.y);
		double X = a.d.s.x + a.d.s.y;
		double Y = b.d.s.x + b.d.s.y;
		double Z = dab_x + dab_y;
		double q = 1 - p;
		return (q*C/A+p*Z/X)/sqrt(q*B/A+p*Y/X);
	} else {
		Integral d = diff_integral(a_,b_);
		double scorei = (d.i.s.x - sqr(d.i.i.x)) + (d.i.s.y - sqr(d.i.i.y));
		double scored = d.d.s.x + d.d.s.y;
		double q = 1 - p;
		return 1 - p*scorei*6  - q*scored/2;
	}
}

Glib::RefPtr<Gdk::Pixbuf> Stroke::draw(int size) const {
	if (size != STROKE_SIZE)
		return draw_(size);
	if (pb)
		return pb;
	pb = draw_(size);
	return pb;
}

Glib::RefPtr<Gdk::Pixbuf> Stroke::pbEmpty;

Glib::RefPtr<Gdk::Pixbuf> Stroke::drawEmpty(int size) {
	if (size != STROKE_SIZE)
		return drawEmpty_(size);
	if (pbEmpty)
		return pbEmpty;
	pbEmpty = drawEmpty_(size);
	return pbEmpty;
}


RStroke Stroke::trefoil() {
	PreStroke s;
	const int n = 40;
	const double pi = 3.141592653589793238462643;
	for (int i = 0; i<=n; i++) {
		double phi = pi*(-4.0*i/n)-2.7;
		double r = exp(1.0 + sin(6.0*pi*i/n)) + 2.0;
		s.add(create_triple(r*cos(phi), r*sin(phi), i));
	}
	return Stroke::create(s, 0, 0);
}
