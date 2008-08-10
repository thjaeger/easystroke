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
#include "annotate.h"
#include <X11/Xutil.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>

DBusConnection *bus;

Annotate::Annotate() {
	DBusError error;
	dbus_error_init (&error);
	bus = dbus_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus)
		exit(EXIT_FAILURE);
}

void Annotate::draw(Point p, Point q) {
	DBusMessage *message = dbus_message_new_method_call (NULL, 
			"/org/freedesktop/compiz/annotate/allscreens/draw", 
			"org.freedesktop.compiz", "activate");
	dbus_message_set_destination (message, "org.freedesktop.compiz");

	const char *Root = "root";
	dbus_int32_t root = ROOT;
	const char *X1 = "x1";
	gdouble x1 = p.x;
	const char *Y1 = "y1";
	gdouble y1 = p.y;
	const char *X2 = "x2";
	gdouble x2 = q.x;
	const char *Y2 = "y2";
	gdouble y2 = q.y;
	dbus_message_append_args (message,
			DBUS_TYPE_STRING, &Root,
			DBUS_TYPE_INT32,  &root,
			DBUS_TYPE_STRING, &X1,
			DBUS_TYPE_DOUBLE, &x1,
			DBUS_TYPE_STRING, &Y1,
			DBUS_TYPE_DOUBLE, &y1,
			DBUS_TYPE_STRING, &X2,
			DBUS_TYPE_DOUBLE, &x2,
			DBUS_TYPE_STRING, &Y2,
			DBUS_TYPE_DOUBLE, &y2,
			DBUS_TYPE_INVALID);
	dbus_connection_send (bus, message, NULL);
	dbus_message_unref (message);
}
void Annotate::end_() {
	DBusMessage *message = dbus_message_new_method_call (NULL, 
			"/org/freedesktop/compiz/annotate/allscreens/clear_key", 
			"org.freedesktop.compiz", "activate");
	dbus_message_set_destination (message, "org.freedesktop.compiz");

	const char *Root = "root";
	dbus_int32_t root = ROOT;
	dbus_message_append_args (message,
			DBUS_TYPE_STRING, &Root,
			DBUS_TYPE_INT32,  &root,
			DBUS_TYPE_INVALID);
	dbus_connection_send (bus, message, NULL);
	dbus_message_unref (message);
}
