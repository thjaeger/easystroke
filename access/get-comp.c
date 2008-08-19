#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cspi/spi.h"

Display *dpy;
#define ROOT (DefaultRootWindow(dpy))

void print_acc(Accessible *obj) {
	const char *name = Accessible_getName(obj);
	const char *role = Accessible_getRoleName(obj);
	printf("%s: %s", role, name);
	AccessibleComponent *comp = Accessible_getComponent(obj);
	if (comp) {
		long int xx, yy, ww, hh;
		AccessibleComponent_getExtents(comp, &xx, &yy, &ww, &hh, SPI_COORD_TYPE_SCREEN);
		printf(" (%ld, %ld, %ld, %ld)", xx, yy, ww, hh);
	}
	printf("\n");
}

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
