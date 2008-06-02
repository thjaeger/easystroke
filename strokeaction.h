#ifndef __STROKEACTION_H__
#define __STROKEACTION_H__
#include "stroke.h"
#include "locking.h"

class StrokeAction : private Lock<boost::shared_ptr<sigc::slot<void, RStroke> > > {
	typedef Ref<boost::shared_ptr<sigc::slot<void, RStroke> > > R;
public:
	bool operator()(RStroke s) {
		R ref(*this);
		if (!(*ref))
			return false;
		(**ref)(s);
		ref->reset();
		return true;
	}

	void erase() {
		R ref(*this);
		ref->reset();
	}

	void set(sigc::slot<void, RStroke> f) {
		R ref(*this);
		ref->reset(new sigc::slot<void, RStroke>(f));
	}
};

StrokeAction& stroke_action();

#endif
