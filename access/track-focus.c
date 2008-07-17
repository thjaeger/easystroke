#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "cspi/spi.h"

static AccessibleEventListener *focus_listener;

void report_focus_event(const AccessibleEvent *event, void *user_data) {
	if (!event->source)
		return;
	char *name = Accessible_getName(event->source);
	char *role = Accessible_getRoleName(event->source);
	if (name && role) {
		printf("%s (%s)\n", name, role);
	} else printf("something went wrong\n");
	if (name)
		SPI_freeString(name);
	if (role)
		SPI_freeString(role);
}

void quit(int sig) {
	SPI_deregisterGlobalEventListenerAll(focus_listener);
	AccessibleEventListener_unref(focus_listener);
	SPI_event_quit();
}

int main (int argc, char **argv) {
	signal(SIGINT, &quit);
	SPI_init();
	focus_listener = SPI_createAccessibleEventListener(report_focus_event, NULL);
	SPI_registerGlobalEventListener(focus_listener, "focus:");
	SPI_event_main();
	return SPI_exit();
}
