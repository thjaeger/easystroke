#ifndef __SHAPE_H__
#define __SHAPE_H__
#include "trace.h"
#include <glibmm/thread.h>

class Shape : public Trace {
	Window win;
private:
	virtual void draw(Point p, Point q);
	virtual void start();
	void clear();
	void defer_clear();
	Glib::Mutex clear_mutex;
public:
	Shape();
	virtual void end();
	virtual ~Shape();
};

#endif
