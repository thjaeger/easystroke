#pragma once
#include <gtkmm.h>
#include "trace.h"
#include <list>

class Popup : public Gtk::Window {
	bool on_draw(const ::Cairo::RefPtr< ::Cairo::Context>& ctx);
	void draw_line(Cairo::RefPtr<Cairo::Context> ctx);
	Gdk::Rectangle rect;
public:
	Popup(int x1, int y1, int x2, int y2);
	void invalidate(int x1, int y1, int x2, int y2);
};

class Composite : public Trace {
	int num_x, num_y;
	Popup ***pieces;
	virtual void draw(Point p, Point q);
	void start_() override;
	void end_() override;
public:
	Composite();
	virtual ~Composite();
};
