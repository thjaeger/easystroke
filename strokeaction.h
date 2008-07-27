#ifndef __STROKEACTION_H__
#define __STROKEACTION_H__
#include "stroke.h"
#include "var.h"

class StrokeAction : private VarE<boost::shared_ptr<sigc::slot<void, RStroke> > > {
	typedef boost::shared_ptr<sigc::slot<void, RStroke> > SA;
public:
	operator bool() {
		return get();
	}
	bool operator()(RStroke stroke) {
		Setter s;
		if (!get())
			return false;
		const SA &sa = s.ref(*this);
		(*sa)(stroke);
		erase();
		return true;
	}

	void erase() {
		Setter s;
		s.set(*this, SA());
	}

	void set(sigc::slot<void, RStroke> f) {
		Setter s;
		s.set(*this, SA(new sigc::slot<void, RStroke>(f)));
	}
};

extern StrokeAction stroke_action;

#endif
