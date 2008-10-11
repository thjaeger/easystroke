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

bool start_dbus() {
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
		return false;

	g_object_unref(proxy);

	obj =(ServerObject *)g_object_new(SERVER_OBJECT_TYPE, NULL);
	dbus_g_connection_register_g_object(connection, "/org/easystroke", G_OBJECT(obj));
	return true;
}
