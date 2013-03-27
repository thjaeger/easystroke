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
#include "gesture.h"
#include "prefdb.h"

void update_triple(RTriple e, int finger, float x, float y, Time t) {
	e->finger = finger;
	e->x = x;
	e->y = y;
	e->t = t;
}

RTriple create_triple(int finger, float x, float y, Time t) {
	RTriple e(new Triple);
	update_triple(e, finger, x, y, t);
	return e;
}

Stroke::Stroke(PreStroke &ps, PreStroke &ps2, int trigger_, int button_, bool timeout_, int multi_finger_) : button(button_), timeout(timeout_), multi_finger(multi_finger_) {
	if(multi_finger >= 1){
		trigger = 0;
	} else {
		trigger = (trigger_ == get_default_button()) ? 0 : trigger_;
	}

	stroke_t *s = NULL;
	if (ps.valid()) {
		s = stroke_alloc(ps.size());
		for (std::vector<RTriple>::iterator i = ps.begin(); i != ps.end(); ++i)
			stroke_add_point(s, (*i)->x, (*i)->y);
	}
	stroke_t *s2 = NULL;
	if (ps2.valid()) {
		s2 = stroke_alloc(ps2.size());
		for (std::vector<RTriple>::iterator i = ps2.begin(); i != ps2.end(); ++i)
			stroke_add_point(s2, (*i)->x, (*i)->y);
	}
	stroke_normalize(s,s2);
	if (ps.valid()) {
		stroke_finish(s);
		stroke.reset(s, &stroke_free);
	}
	if (ps2.valid()) {
		stroke_finish(s2);
		stroke2.reset(s2, &stroke_free);
	}
}

int Stroke::compare(RStroke a, RStroke b, double &score) {
	score = 0.0;
	if (!a || !b)
		return -1;
	if (!a->timeout != !b->timeout)
		return -1;
	if (a->button != b->button)
		return -1;
	if (a->multi_finger != b->multi_finger){
		if (verbosity >= 2) printf("different number of finger seeking=%d compared with=%d\n", a->multi_finger,b->multi_finger);
		return -1;
	}
	if (a->trigger != b->trigger) {
		if (a->trigger && b->trigger)
			return -1;
		if (a->trigger + b->trigger != get_default_button())
			return -1;
	}
	if (!a->stroke || !b->stroke) {
		if (!a->stroke && !b->stroke) {
			score = 1.0;
			return 1;
		}
		else
			return -1;
	}
	double cost;
	if (a->multi_finger != 2) {
		if (verbosity >= 2) printf("finger %d more than 2\n", a->multi_finger);
		cost = stroke_compare(a->stroke.get(), b->stroke.get(), NULL, NULL);
		if (verbosity >= 2) printf("compared cost=%g\n", cost);
	} else {
		if (verbosity >= 2) printf("finger %d equals 2\n", a->multi_finger);
		double how_compatible_1_1 = stroke_how_compatible( a->stroke.get(), b->stroke.get() );
		double how_compatible_2_2 = stroke_how_compatible( a->stroke2.get(), b->stroke2.get() );
		double how_compatible_1_2 = stroke_how_compatible( a->stroke.get(), b->stroke2.get() );
		double how_compatible_2_1 = stroke_how_compatible( a->stroke2.get(), b->stroke.get() );
		double how_compatible_e = ( how_compatible_1_1 + how_compatible_2_2 ) / 2.0;
		double how_compatible_r = ( how_compatible_1_2 + how_compatible_2_1 ) / 2.0;
		double how_compatible;
		bool compatible_e = (how_compatible_1_1 > 0.5 && how_compatible_2_2 > 0.5);
		bool compatible_r = (how_compatible_1_2 > 0.5 && how_compatible_2_1 > 0.5);
		stroke_t *stroke = NULL;
		stroke_t *stroke2 = NULL;
		if (! compatible_e && ! compatible_r) {
			return -1;
		} else if(compatible_e && ! compatible_r) {
			stroke = a->stroke.get();
			stroke2 = a->stroke2.get();
			how_compatible = how_compatible_e;
		} else if(! compatible_e && compatible_r) {
			stroke = a->stroke2.get();
			stroke2 = a->stroke.get();
			how_compatible = how_compatible_r;
		} else if( how_compatible_1_1 + how_compatible_2_2 >= how_compatible_1_2 + how_compatible_2_1) {
			stroke = a->stroke.get();
			stroke2 = a->stroke2.get();
			how_compatible = how_compatible_e;
		} else {
			stroke = a->stroke2.get();
			stroke2 = a->stroke.get();
			how_compatible = how_compatible_r;
		}
		double compatible_factor;
		double cost_1_1 = 0.0, cost_2_2 = 0.0;
		cost_1_1 = stroke_compare(stroke, b->stroke.get(), NULL, NULL);
		cost_2_2 = stroke_compare(stroke2, b->stroke2.get(), NULL, NULL);
		compatible_factor = 2.0 / ( 1.1 - how_compatible );
		cost = ( cost_1_1 + cost_2_2 ) / 2.0 * compatible_factor;
		if (verbosity >= 2) printf("compared cost=%g cost1_1=%g cost2_2=%g fact=%g\n", cost, cost_1_1, cost_2_2, compatible_factor);
	}
	if (cost >= stroke_infinity)
		return -1;
	score = MAX(1.0 - 2.5*cost, 0.0);
		if (verbosity >= 2) printf("score=%g\n", score);
	if (a->timeout)
		return score > 0.85;
	else
		return score > 0.7;
}

Glib::RefPtr<Gdk::Pixbuf> Stroke::draw(int size, double width, bool inv) const {
	if (size != STROKE_SIZE || (width != 2.0 && width != 4.0) || inv)
		return draw_(size, width, inv);
	int i = width == 2.0;
	if (pb[i])
		return pb[i];
	pb[i] = draw_(size, width);
	return pb[i];
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
	PreStroke s2;
	const int n = 40;
	for (int i = 0; i<=n; i++) {
		double phi = M_PI*(-4.0*i/n)-2.7;
		double r = exp(1.0 + sin(6.0*M_PI*i/n)) + 2.0;
		s.add(create_triple(0,r*cos(phi), r*sin(phi), i));
	}
	return Stroke::create(s, s2, 0, 0, false, 1);
}
