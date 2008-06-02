#ifndef __TRACE_H__
#define __TRACE_H__

#include <X11/Xlib.h>

#define WIDTH 4
#define TRACE_COLOR 0x980101

class Trace {
public:
	struct Point { int x; int y; };
private:
	Point last;
protected:
	virtual void draw(Point p, Point q) = 0;
	virtual void start() = 0;
public:
	void start(Point p) { last = p; start(); }
	virtual void end() = 0;
	void draw(Point p) { draw(last, p); last = p; }
	virtual ~Trace() {}
};

class Trivial : public Trace {
	virtual void draw(Point p, Point q) {}
	virtual void start() {}
public:
	virtual void end() {}
};

#endif
