/*
 * Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __TRACE_H__
#define __TRACE_H__

#include <X11/Xlib.h>
#include <exception>
#include <glibmm/i18n.h>

struct DBusException: public std::exception {
	virtual const char* what() const throw() { return _("Connection to DBus failed"); }
};

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
