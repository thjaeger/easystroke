#pragma once

#include <memory>

class Trace {
public:
	struct Point { float x; float y; };
private:
	Point last;
	bool active;
protected:
	virtual void draw(Point p, Point q) = 0;
	virtual void start_() = 0;
	virtual void end_() = 0;
public:
	Trace() : active(false) {}
	void draw(Point p) { draw(last, p); last = p; }
	void start(Point p);
	void end();
	virtual void timeout() {}
	virtual ~Trace() {}
};

void resetTrace();

extern std::shared_ptr<Trace> trace;
