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
	bool active;
protected:
	virtual void draw(Point p, Point q) = 0;
	virtual void start_() = 0;
	virtual void end_() = 0;
public:
	Trace() : active(false) {}
	void start(Point p) { last = p; active = true; start_(); }
	void end() { if (!active) return; active = false; end_(); }
	void draw(Point p) { draw(last, p); last = p; }
	virtual void timeout() {}
	virtual ~Trace() {}
};

class Trivial : public Trace {
	virtual void draw(Point p, Point q) {}
	virtual void start_() {}
	virtual void end_() {}
public:
};

#endif
