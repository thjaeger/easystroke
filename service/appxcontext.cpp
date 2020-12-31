#include "appxcontext.h"
#include <xorg/xserver-properties.h>

AppXContext::AppXContext() {
    this->dpy = XOpenDisplay(nullptr);
    if (!this->dpy) {
        g_error("Couldn't open display.");
    }

    this->ROOT = DefaultRootWindow(dpy);

    NET_FRAME_WINDOW = internAtom("_NET_FRAME_WINDOW");
    NET_WM_STATE = internAtom("_NET_WM_STATE");
    NET_WM_STATE_HIDDEN = internAtom("_NET_WM_STATE_HIDDEN");
    NET_ACTIVE_WINDOW = internAtom("_NET_ACTIVE_WINDOW");
    NET_WM_WINDOW_TYPE = internAtom("_NET_WM_WINDOW_TYPE");
    NET_WM_WINDOW_TYPE_DOCK = internAtom("_NET_WM_WINDOW_TYPE_DOCK");
    WM_PROTOCOLS = internAtom("WM_PROTOCOLS");
    WM_TAKE_FOCUS = internAtom("WM_TAKE_FOCUS");

    XTEST = internAtom(XI_PROP_XTEST_DEVICE);
    PROXIMITY = internAtom(AXIS_LABEL_PROP_ABS_DISTANCE);
    WM_STATE = internAtom("WM_STATE");
    EASYSTROKE_PING = internAtom("EASYSTROKE_PING");
}

AppXContext::~AppXContext() {
    XCloseDisplay(this->dpy);
}

Atom AppXContext::internAtom(std::string name) {
    return XInternAtom(dpy, name.c_str(), false);
}
