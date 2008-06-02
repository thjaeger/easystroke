#ifndef __COPY_H__
#define __COPY_H__
#include "trace.h"
#include "main.h"

class Copy : public Trace {
	Window win;
	GC gc;
private:
	virtual void draw(Point p, Point q) { XDrawLine(dpy, win, gc, p.x, p.y, q.x, q.y); }
	virtual void start() { XMapRaised(dpy, win); }
public:
	Copy();
	virtual void end() { XUnmapWindow(dpy, win); }
	virtual ~Copy() { XFreeGC(dpy, gc); XDestroyWindow(dpy, win); }
};

#endif
