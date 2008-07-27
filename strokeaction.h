#ifndef __STROKEACTION_H__
#define __STROKEACTION_H__
#include "stroke.h"
#include "var.h"

class StrokeAction : private Var<boost::shared_ptr<sigc::slot<void, RStroke> > > {
	typedef boost::shared_ptr<sigc::slot<void, RStroke> > SA;
public:
	operator bool() {
		return get();
	}
	bool operator()(RStroke stroke) {
		Setter s;
		SA &sa = s.ref(*this);
		if (!sa)
			return false;
		(*sa)(stroke);
		sa.reset();
		return true;
	}

	void erase() {
		Setter s;
		s.ref(*this).reset();
	}

	void set(sigc::slot<void, RStroke> f) {
		Setter s;
		s.ref(*this).reset(new sigc::slot<void, RStroke>(f));
	}
};

extern StrokeAction stroke_action;

#endif
