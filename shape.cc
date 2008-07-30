#include "shape.h"
#include "main.h"

#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

Shape::Shape() {
	int screen = DefaultScreen(dpy);
	Window root = RootWindow(dpy, screen);
	int w = DisplayWidth(dpy, screen);
	int h = DisplayHeight(dpy, screen);
	unsigned long bg = TRACE_COLOR;
	win = XCreateSimpleWindow(dpy, root, 0, 0, w, h, 0, CopyFromParent, bg);
	XSetWindowAttributes attr;
	attr.override_redirect = True;
	XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &attr);

	clear();
}

#define min(x,y) ((x) > (y) ? (y) : (x))
#define abs(x) ((x) > 0 ? (x) : -(x))

void Shape::draw(Point p, Point q) {
	int x = min(p.x, q.x) - WIDTH;
	int y = min(p.y, q.y) - WIDTH;
	int w = abs(p.x - q.x) + 2*WIDTH;
	int h = abs(p.y - q.y) + 2*WIDTH;
	Pixmap pm = XCreatePixmap(dpy, DefaultRootWindow(dpy), w, h, 1);

	XGCValues gcv;
	gcv.foreground = 0;
	gcv.line_width = WIDTH;
	gcv.cap_style = CapRound;
	GC gc = XCreateGC(dpy, pm, GCCapStyle | GCForeground | GCLineWidth, &gcv);
	XFillRectangle(dpy, pm, gc, 0, 0, w, h);
	XSetForeground(dpy, gc, 1);
	XDrawLine(dpy, pm, gc, p.x-x, p.y-y, q.x-x, q.y-y);
	XFreeGC(dpy, gc);

	XShapeCombineMask(dpy, win, ShapeBounding, x, y, pm, ShapeUnion);
	XFreePixmap(dpy, pm);
}

void Shape::start_() {
	if (remove_timeout(1))
		clear();
	XMapRaised(dpy, win);
}

void Shape::end_() {
	XUnmapWindow(dpy, win);
	set_timeout(1,100*1000);
}

void Shape::timeout() {
	clear();
}

void Shape::clear() {
	XShapeCombineRectangles(dpy, win, ShapeBounding, 0, 0, NULL, 0, ShapeSet, YXBanded);
}

Shape::~Shape() {
	XDestroyWindow(dpy, win);
}
