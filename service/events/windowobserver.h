#pragma once

#include "gesture.h"
#include "grabber.h"
#include "actiondb.h"

#include <gesture.h>
#include "var.h"

namespace Events {
    extern Source<Window> current_app_window;

    Window getAppWindow(Window w);

    class WindowObserver {
    public:
        void handleEnterLeave(XEvent &ev);

        void handlePropertyNotify(XEvent &ev);

        static void tryActivateCurrentWindow(Time t);
    };
}
