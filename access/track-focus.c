/*
 * Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "cspi/spi.h"
#include <map>
#include <string>
#include <iostream>

static AccessibleEventListener *focus_listener;
static AccessibleEventListener *activate_listener;
static AccessibleEventListener *deactivate_listener;

int stack[4];
int length;

void push(int x) {
	for (int i=0; i<length; i++)
		if (stack[i] == x)
			return;
	for (int i=3; i>0; i--)
		stack[i] = stack[i-1];
	stack[0] = x;
	if (length < 4)
		length++;
}

void pop(int x) {
	for (int i=0; i<length; i++)
		if (stack[i] == x) {
			for (int j=i; j<3; j++)
				stack[j] = stack[j+1];
			length--;
		}
}

std::map<int, std::string> focus_map;

void print() {
	static std::string last = "";
	std::string now = length ? focus_map[stack[0]] : "no app or app doesn't support at-spi";
	if (now == "")
		now = "no focus information about this app yet";
	if (now == last)
		return;
	last = now;
	std::cout << now << std::endl;
}

void on_focus(const AccessibleEvent *event, void *user_data) {
	if (!event->source)
		return;
	AccessibleApplication *app = Accessible_getHostApplication(event->source);
	char *name = Accessible_getName(event->source);
	char *role = Accessible_getRoleName(event->source);
	if (app && name && role) {
		int app_id = AccessibleApplication_getID(app);
		focus_map[app_id] = std::string(name) + "(" + std::string(role) + ")";
		push(app_id);
		print();
	} else printf("something went wrong\n");
	if (app) {
		AccessibleApplication_unref(app);
		AccessibleApplication_unref(app);
	}
	if (name)
		SPI_freeString(name);
	if (role)
		SPI_freeString(role);
}

void on_activate(const AccessibleEvent *event, void *user_data) {
	if (!event->source)
		return;
	AccessibleApplication *app = Accessible_getHostApplication(event->source);
	if (!app)
		return;
	push(AccessibleApplication_getID(app));
	print();
	AccessibleApplication_unref(app);
	AccessibleApplication_unref(app);
}

void on_deactivate(const AccessibleEvent *event, void *user_data) {
	if (!event->source)
		return;
	AccessibleApplication *app = Accessible_getHostApplication(event->source);
	if (!app)
		return;
	pop(AccessibleApplication_getID(app));
	print();
	AccessibleApplication_unref(app);
	AccessibleApplication_unref(app);
}

void quit(int sig) {
	SPI_deregisterGlobalEventListenerAll(focus_listener);
	SPI_deregisterGlobalEventListenerAll(activate_listener);
	SPI_deregisterGlobalEventListenerAll(deactivate_listener);
	AccessibleEventListener_unref(focus_listener);
	AccessibleEventListener_unref(activate_listener);
	AccessibleEventListener_unref(deactivate_listener);
	SPI_event_quit();
}

int main (int argc, char **argv) {
	signal(SIGINT, &quit);
	SPI_init();
	focus_listener = SPI_createAccessibleEventListener(on_focus, NULL);
	activate_listener = SPI_createAccessibleEventListener(on_activate, NULL);
	deactivate_listener = SPI_createAccessibleEventListener(on_deactivate, NULL);
	SPI_registerGlobalEventListener(focus_listener, "focus:");
	SPI_registerGlobalEventListener(activate_listener, "window:activate");
	SPI_registerGlobalEventListener(deactivate_listener, "window:deactivate");
	SPI_event_main();
	return SPI_exit();
}
