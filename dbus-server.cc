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
#include <dbus/dbus-protocol.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>

#include <stdio.h>

#define SERVER_OBJECT_TYPE (server_object_get_type())

typedef struct _ServerObject {
	GObject object;
} ServerObject;

typedef struct _ServerObjectClass {
	GObjectClass object_class;
} ServerObjectClass;

void run_by_name(const char *str);

static gboolean server_send(ServerObject *obj, const char *str, GError **error) {
	run_by_name(str);
	return TRUE;
}

#include "dbus-server.h"

static void server_object_class_init(ServerObjectClass *klass) {
	dbus_g_object_type_install_info(G_TYPE_FROM_CLASS(klass), &dbus_glib_server_object_info);
}

static void server_object_init(ServerObject *obj) {}

G_DEFINE_TYPE(ServerObject, server_object, G_TYPE_OBJECT);

int start_dbus() {
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = NULL;
	ServerObject *obj;
	guint32 request_name_ret;

	connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (!connection)
		return false;

	proxy = dbus_g_proxy_new_for_name(connection, DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
	if (!org_freedesktop_DBus_request_name(proxy, "org.easystroke", 0, &request_name_ret, &error))
		return false;

	if (request_name_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		return -1;

	g_object_unref(proxy);

	obj =(ServerObject *)g_object_new(SERVER_OBJECT_TYPE, NULL);
	dbus_g_connection_register_g_object(connection, "/org/easystroke", G_OBJECT(obj));
	return true;
}
