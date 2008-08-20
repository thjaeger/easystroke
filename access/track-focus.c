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
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <cspi/spi.h>
#include <map>
#include <string>
#include <iostream>

// TODO: Leaks memory
void print_acc(Accessible *obj) {
	char *name = Accessible_getName(obj);
	char *role = Accessible_getRoleName(obj);
	printf("%s: %s", role, name);
	SPI_freeString(name);
	SPI_freeString(role);

	AccessibleComponent *comp = Accessible_getComponent(obj);
	if (comp) {
		long int xx, yy, ww, hh;
		AccessibleComponent_getExtents(comp, &xx, &yy, &ww, &hh, SPI_COORD_TYPE_SCREEN);
		printf(" (%ld, %ld, %ld, %ld)", xx, yy, ww, hh);
	}
	AccessibleValue *value = Accessible_getValue(obj);
	if (value) {
		printf(" (value: %f [%f - %f])\n", 
				AccessibleValue_getCurrentValue(value),
				AccessibleValue_getMinimumValue(value),
				AccessibleValue_getMaximumValue(value));
	}
	AccessibleDocument *doc = Accessible_getDocument(obj);
	if (doc) {
		AccessibleAttributeSet *attrs = AccessibleDocument_getAttributes(doc);
		printf(" (document");
		if (attrs->len != 0) {
			printf(": ");
			for (int i = 0;;) {
				printf("%s", attrs->attributes[i]);
				if (++i >= attrs->len)
					break;
				printf(", ");
			}
		}
		printf(")");

	}
	AccessibleAction *act = Accessible_getAction(obj);
	if (act) {
		long n = AccessibleAction_getNActions(act);
		printf(" (action");
		if (n != 0) {
			printf(": ");
			for (int i = 0;;) {
				printf("%s", AccessibleAction_getName(act, i));
				if (++i >= n)
					break;
				printf(", ");
			}
		}
		printf(")");

	}
	AccessibleHypertext *hyp = Accessible_getHypertext(obj);
	if (hyp) {
		AccessibleText *txt = Accessible_getText(obj);
		printf(" (link: %s)", AccessibleText_getText(txt, 0, AccessibleText_getCharacterCount(txt)));

	}
	printf("\n");
}

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

std::map<int, Accessible *> focus_map;

void print() {
	if (!length) {
		printf("no app or app doesn't support at-spi\n");
		return;
	}
	Accessible *acc = focus_map[stack[0]];
	if (!acc) {
		printf("no focus information about this app yet");
		return;
	}
	print_acc(acc);
}

void on_focus(const AccessibleEvent *event, void *user_data) {
	if (!event->source)
		return;
	AccessibleApplication *app = Accessible_getHostApplication(event->source);
	if (app) {
		int app_id = AccessibleApplication_getID(app);
		focus_map[app_id] = event->source;
		push(app_id);
		print();
	} else printf("something went wrong\n");
	if (app) {
		AccessibleApplication_unref(app);
		AccessibleApplication_unref(app);
	}
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
	if (SPI_init()) {
		printf("Error: AT-SPI not available\n");
		exit(EXIT_FAILURE);
	};
	signal(SIGINT, &quit);
	focus_listener = SPI_createAccessibleEventListener(on_focus, NULL);
	activate_listener = SPI_createAccessibleEventListener(on_activate, NULL);
	deactivate_listener = SPI_createAccessibleEventListener(on_deactivate, NULL);
	SPI_registerGlobalEventListener(focus_listener, "focus:");
	SPI_registerGlobalEventListener(activate_listener, "window:activate");
	SPI_registerGlobalEventListener(deactivate_listener, "window:deactivate");
	SPI_event_main();
	return SPI_exit();
}

#if 0
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cspi/spi.h>
#include "print.h"

Display *dpy;
#define ROOT (DefaultRootWindow(dpy))

Accessible *get_inf(Accessible *cur, int x, int y) {
	for (;;) {
		AccessibleComponent *comp = Accessible_getComponent(cur);
		if (!comp)
			return cur;
		Accessible *child = AccessibleComponent_getAccessibleAtPoint(comp, x, y, SPI_COORD_TYPE_SCREEN);
		if (!child)
			return cur;
		cur = child;
	}
}

void get_comp(int x, int y) {
	Accessible *desktop = SPI_getDesktop(0);
	int n = Accessible_getChildCount(desktop);
	for (int i = 0; i < n; i++) {
		Accessible *app = Accessible_getChildAtIndex(desktop, i);
		if (strcmp(Accessible_getName(app),"Firefox"))
			continue;
		print_acc(app);
		int m = Accessible_getChildCount(app);
		for (int j = 0; j < m; j++) {
			Accessible *child = Accessible_getChildAtIndex(app, j);
			Accessible *inf = get_inf(child, x, y);
			if (inf != child)
				print_acc(inf);
		}
	}
};

void print_subtree(Accessible *cur, int indent) {
	for (int i = 0; i < indent; i++)
		printf(" ");
	print_acc(cur);
	int n = Accessible_getChildCount(cur);
	for (int i = 0; i < n; i++)
		print_subtree(Accessible_getChildAtIndex(cur, i), indent+2);
}

void print_tree() {
	print_subtree(SPI_getDesktop(0), 0);
}

int main(int argc, char *argv[]) {
	dpy = XOpenDisplay(NULL);

	if (SPI_init()) {
		printf("Error: AT-SPI not available\n");
		exit(EXIT_FAILURE);
	};
	if (1) {
		print_tree();
		exit(EXIT_SUCCESS);
	}

	XGrabButton(dpy, 1, AnyModifier, ROOT, True, ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);

	XEvent ev;
	while (1) {
		XNextEvent(dpy, &ev);
		if (ev.type == ButtonPress) {
			XAllowEvents(dpy, ReplayPointer, ev.xbutton.time);
			get_comp(ev.xbutton.x, ev.xbutton.y);
		}
	}
}
#endif
