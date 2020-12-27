/*
 * Copyright (c) 2008-2009, Thomas Jaeger <ThJaeger@gmail.com>
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
#include "water.h"
#include <X11/Xlib.h>
#include <stdio.h>

Water::Water() {
	const char *ofc = "org.freedesktop.compiz";
	GError *error = 0;
	bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_error_free(error);
		throw DBusException();
	}
	char line[256];
	snprintf(line, sizeof(line), "/org/freedesktop/compiz/water/screen%d/line", DefaultScreen(dpy));
	line_proxy = dbus_g_proxy_new_for_name(bus, ofc, line, ofc);
}

void Water::draw(Point p, Point q) {
	dbus_g_proxy_call_no_reply(line_proxy, "activate",
			G_TYPE_STRING, "root", G_TYPE_INT, gint(ROOT),
			G_TYPE_STRING, "x0",   G_TYPE_INT, gint32(p.x),
			G_TYPE_STRING, "y0",   G_TYPE_INT, gint32(p.y),
			G_TYPE_STRING, "x1",   G_TYPE_INT, gint32(q.x),
			G_TYPE_STRING, "y1",   G_TYPE_INT, gint32(q.y),
			G_TYPE_INVALID);
}
