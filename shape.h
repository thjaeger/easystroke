#ifndef __SHAPE_H__
#define __SHAPE_H__
#include "trace.h"
#include <glibmm/thread.h>

class Shape : public Trace {
	Window win;
private:
	virtual void draw(Point p, Point q);
	virtual void start_();
	virtual void end_();
	void clear();
public:
	Shape();
	virtual void timeout();
	virtual ~Shape();
};

#endif
