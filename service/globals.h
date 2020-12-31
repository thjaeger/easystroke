#pragma once

#include <gtkmm.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>

extern "C" {
    struct _XDisplay;
    typedef struct _XDisplay Display;
}

extern Display *dpy;
extern Window ROOT;
