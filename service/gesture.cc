#include "gesture.h"
#include "prefdb.h"

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

Stroke::Stroke(PreStroke &ps, int trigger_, int button_, unsigned int modifiers_, bool timeout_)
    : trigger(trigger_), button(button_), modifiers(modifiers_), timeout(timeout_) {
	if (ps.valid()) {
        stroke = std::make_shared<Stroke2>();
        for (auto i : ps) {
            stroke->addPoint(i->x, i->y);
        }

        stroke->finish();
    }
}

int Stroke::compare(RStroke a, RStroke b, double &score) {
    score = 0.0;
    if (!a || !b) {
        return -1;
    }
    if (!a->timeout != !b->timeout) {
        return -1;
    }
    if (a->button != b->button) {
        return -1;
    }
    if (a->trigger != b->trigger) {
        return -1;
    }
    if (a->modifiers != b->modifiers) {
        return -1;
    }
    if (!a->stroke || !b->stroke) {
        if (!a->stroke && !b->stroke) {
            score = 1.0;
            return 1;
        }
        return -1;
    }
    double cost = Stroke2::compare(*a->stroke, *b->stroke, nullptr, nullptr);
    if (cost >= stroke_infinity) {
        return -1;
    }
    score = MAX(1.0 - 2.5 * cost, 0.0);
    if (a->timeout) {
        return score > 0.85;
    } else {
        return score > 0.7;
    }
}
