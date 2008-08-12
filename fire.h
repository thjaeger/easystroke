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
#ifndef __FIRE_H__
#define __FIRE_H__
#include "util.h"
#include "trace.h"
#include "main.h"
#include <dbus/dbus-glib.h>

class Fire : public Trace, public Timeout {
	DBusGConnection *bus;
	DBusGProxy *point_proxy;
	DBusGProxy *clear_proxy;
	float leftover;

	virtual void draw(Point p, Point q);
	void add_point(float, float);
	virtual void start_() { leftover = 0; }
	virtual void end_() { set_timeout(200); }
	virtual void timeout();
public:
	Fire();
};

#endif
