#pragma once

#include "gesture.h"
#include "grabber.h"
#include "actiondb.h"

#include <gesture.h>

namespace Events {
    class BasicApplicationInfo;

    class WindowObserver {
    private:
        std::unique_ptr<BasicApplicationInfo> currentApplication;

        void setCurrentApplication(Window applicationWindow);

        Window parent;

    public:
        explicit WindowObserver(Window window);

        ~WindowObserver();

        void handleEnterLeave(XEvent &ev);

        void handlePropertyNotify(XEvent &ev);

        void tryActivateCurrentWindow(Time t);

        std::string getCurrentWindowClass();

        void setCurrentWindow(Window newWindow);

        bool handle(XEvent &ev);

        void add(Window);

        void remove(Window);

        void destroy(Window);
    };
}
