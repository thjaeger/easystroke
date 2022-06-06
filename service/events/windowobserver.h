#pragma once

#include "gesture.h"
#include "grabber.h"
#include "actiondb.h"

#include <gesture.h>

namespace Events {
    class BasicApplicationInfo;

    /**
     * Keeps track of all the top level windows (applications) in the XServer, and which one is currently "active" (under the pointer)
     */
    class WindowObserver {
    private:
        std::unique_ptr<BasicApplicationInfo> currentApplication;

        void setCurrentApplication(Window applicationWindow);

        Window parent;

        void add(Window);

        void remove(Window);

        void destroy(Window);

    public:
        explicit WindowObserver(Window window);

        ~WindowObserver();

        void handleEnterLeave(XEvent &ev);

        void handlePropertyNotify(XEvent &ev);

        void tryActivateCurrentWindow(Time t);

        std::string getCurrentWindowClass();

        void setCurrentWindow(Window newWindow);

        bool handle(XEvent &ev);
    };
}
