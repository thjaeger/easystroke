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
#include "annotate.h"

Annotate::Annotate() {
	const char *ofc = "org.freedesktop.compiz";
	GError *error = 0;
	bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_error_free(error);
		throw DBusException();
	}
	draw_proxy = dbus_g_proxy_new_for_name(bus, ofc, "/org/freedesktop/compiz/annotate/allscreens/draw", ofc);
	clear_proxy = dbus_g_proxy_new_for_name(bus, ofc, "/org/freedesktop/compiz/annotate/allscreens/clear_key", ofc);
}

void Annotate::draw(Point p, Point q) {
	dbus_g_proxy_call_no_reply(draw_proxy, "activate",
			G_TYPE_STRING, "root", G_TYPE_INT,    gint(ROOT),
			G_TYPE_STRING, "x1",   G_TYPE_DOUBLE, gdouble(p.x),
			G_TYPE_STRING, "y1",   G_TYPE_DOUBLE, gdouble(p.y),
			G_TYPE_STRING, "x2",   G_TYPE_DOUBLE, gdouble(q.x),
			G_TYPE_STRING, "y2",   G_TYPE_DOUBLE, gdouble(q.y),
			G_TYPE_INVALID);
}
void Annotate::end_() {
	dbus_g_proxy_call_no_reply(clear_proxy, "activate",
			G_TYPE_STRING, "root", G_TYPE_INT,    gint(ROOT),
			G_TYPE_INVALID);
}
