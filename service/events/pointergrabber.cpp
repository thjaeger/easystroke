#include "pointergrabber.h"

#include <X11/cursorfont.h>

#include "xserverproxy.h"


namespace Events {
    PointerGrabber::PointerGrabber() : cursor_select(global_xServer->createFontCursor(XC_crosshair)) {}

    PointerGrabber::~PointerGrabber() {
        global_xServer->freeCursor(cursor_select);
    }

    void PointerGrabber::grab() {
        g_debug("Grabbing pointer");
        int code = global_xServer->grabPointer(global_xServer->ROOT, False, ButtonPressMask,
                                               GrabModeAsync, GrabModeAsync, global_xServer->ROOT, cursor_select,
                                               CurrentTime);
        if (code != GrabSuccess) {
            throw GrabFailedException(code);
        }
    }

    void PointerGrabber::ungrab() {
        g_debug("Ungrabbing pointer");
        global_xServer->ungrabPointer(CurrentTime);
    }
}
