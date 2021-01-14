#pragma once

#include <gtkmm.h>
#include <map>
#include <string>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>

namespace Events {
    class PointerGrabber {
    private:
        Cursor cursor_select;

    public:
        PointerGrabber();

        ~PointerGrabber();

        void grab();

        void ungrab();
    };

    class GrabFailedException : public std::exception {
        char *msg;
    public:
        GrabFailedException(int code) { if (asprintf(&msg, "Grab Failed: %d", code) == -1) msg = nullptr; }

        [[nodiscard]] const char *what() const throw() override { return msg ? msg : "Grab Failed"; }

        ~GrabFailedException() throw() { free(msg); }
    };
}
