#pragma once

#include <gtkmm.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>

extern "C" {
    struct _XDisplay;
    typedef struct _XDisplay Display;
}

class AppXContext {
private:
    Atom internAtom(std::string name);

public:
    AppXContext();
    ~AppXContext();

    Display* dpy;
    Window ROOT;

    Atom NET_FRAME_WINDOW;
    Atom NET_WM_STATE;
    Atom NET_WM_STATE_HIDDEN;
    Atom NET_ACTIVE_WINDOW;
    Atom XTEST;
    Atom PROXIMITY;
    Atom WM_STATE;
    Atom NET_WM_WINDOW_TYPE;
    Atom NET_WM_WINDOW_TYPE_DOCK;
    Atom WM_PROTOCOLS;
    Atom WM_TAKE_FOCUS;
};
