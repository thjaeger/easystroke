#include "copy.h"
#include <X11/Xutil.h>

Copy::Copy() {
	int screen = DefaultScreen(dpy);
	Window root = RootWindow(dpy, screen);
	int w = DisplayWidth(dpy, screen);
	int h = DisplayHeight(dpy, screen);
	win = XCreateSimpleWindow(dpy, root, 0, 0, w, h, 0, CopyFromParent, CopyFromParent);
	XSetWindowAttributes attr;
	attr.override_redirect = True;
	attr.save_under = True;
	attr.background_pixmap = None;
	XChangeWindowAttributes(dpy, win, CWOverrideRedirect | CWSaveUnder | CWBackPixmap, &attr);

	XGCValues gcv;
	gcv.foreground = TRACE_COLOR;
	gcv.line_width = WIDTH;
	gc = XCreateGC(dpy, win, GCForeground | GCLineWidth, &gcv);
}
