#pragma once

#include "gesture.h"
#include "grabber.h"
#include "actiondb.h"

#include <gesture.h>
#include "var.h"

namespace Events {
    class WindowObserver {
    public:
        // TODO: Make private
        Source<Window> currentAppWindow;
        Out<std::string> *current_class;

        WindowObserver();

        void handleEnterLeave(XEvent &ev);

        void handlePropertyNotify(XEvent &ev);

        void tryActivateCurrentWindow(Time t);

        std::string getCurrentWindowClass();

        void setCurrentWindow(Window newWindow);
    };
}
