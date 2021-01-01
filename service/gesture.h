#pragma once

#include "stroke.h"
#include <gdkmm.h>
#include <vector>

#include <X11/X.h>

class Stroke;
class PreStroke;

typedef std::shared_ptr<Stroke> RStroke;
typedef std::shared_ptr<PreStroke> RPreStroke;

struct Triple {
	float x;
	float y;
	Time t;
};
typedef std::shared_ptr<Triple> RTriple;
void update_triple(RTriple e, float x, float y, Time t);
RTriple create_triple(float x, float y, Time t);

class Stroke {
public:
	int trigger;
	int button;
	unsigned int modifiers;
	bool timeout;
	std::shared_ptr<Stroke2> stroke;

	Stroke() : trigger(0), button(0), modifiers(AnyModifier), timeout(false) {}
    Stroke(PreStroke &s, int trigger_, int button_, unsigned int modifiers_, bool timeout_);

	static int compare(RStroke, RStroke, double &);

	unsigned int size() const { return stroke ? stroke->size() : 0; }
	bool trivial() const { return size() == 0 && button == 0; }
};

class PreStroke : public std::vector<RTriple> {
public:
	static RPreStroke create() { return RPreStroke(new PreStroke()); }
	void add(RTriple p) { push_back(p); }
	bool valid() const { return size() > 2; }
};
