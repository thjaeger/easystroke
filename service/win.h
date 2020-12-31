#pragma once
#include "gesture.h"
#include <gtkmm.h>
#include "util.h"
#include "prefdb.h"

class Ranking;

// Convenience macro for on-the-fly creation of widgets
#define WIDGET(TYPE, NAME, ARGS...) TYPE &NAME = *Gtk::manage(new TYPE(ARGS))

extern Glib::RefPtr<Gtk::Builder> widgets;
