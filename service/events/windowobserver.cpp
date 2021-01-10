#include "windowobserver.h"
#include "grabber.h"
#include "xserverproxy.h"

namespace Events {
    Source<Window> current_app_window(None);

    Window getAppWindow(Window w) {
        return grabbers::get_app_window(w);
    }

    void WindowObserver::handleEnterLeave(XEvent &ev) {
        if (ev.xcrossing.mode == NotifyGrab) {
            return;
        }

        if (ev.xcrossing.detail == NotifyInferior) {
            return;
        }

        if (ev.type == EnterNotify) {
            auto w = ev.xcrossing.window;
            current_app_window.set(Events::getAppWindow(w));
            g_debug("Entered window 0x%lx -> 0x%lx", w, current_app_window.get());
        } else {
            g_warning("Error: Bogus Enter/Leave event");
        };
    }

    void WindowObserver::handlePropertyNotify(XEvent &ev) {
        if (current_app_window.get() == ev.xproperty.window &&
            ev.xproperty.atom == XA_WM_CLASS) {
            current_app_window.notify();
        }
    }

    void WindowObserver::tryActivateCurrentWindow(Time t) {
        if (!Events::current_app_window.get()) {
            return;
        }

        auto w = Events::current_app_window.get();
        if (w == global_xServer->getWindow(global_xServer->ROOT, global_xServer->atoms.NET_ACTIVE_WINDOW)) {
            return;
        }

        auto window_type = global_xServer->getAtom(w, global_xServer->atoms.NET_WM_WINDOW_TYPE);
        if (window_type == global_xServer->atoms.NET_WM_WINDOW_TYPE_DOCK) {
            return;
        }

        auto *wm_hints = global_xServer->getWMHints(w);
        if (wm_hints) {
            bool input = wm_hints->input;
            XServerProxy::free(wm_hints);
            if (!input) {
                return;
            }
        }

        if (!global_xServer->hasAtom(w, global_xServer->atoms.WM_PROTOCOLS, global_xServer->atoms.WM_TAKE_FOCUS)) {
            return;
        }

        XWindowAttributes attr;
        if (global_xServer->getWindowAttributes(w, &attr) && attr.override_redirect) {
            return;
        }

        g_debug("Giving focus to window 0x%lx", w);

        auto a = global_xServer->atoms.WM_TAKE_FOCUS;
        XClientMessageEvent ev = {
                .type =ClientMessage,
                .window = w,
                .message_type = global_xServer->atoms.WM_PROTOCOLS,
                .format = 32,
        };

        ev.data.l[0] = a;
        ev.data.l[1] = t;

        global_xServer->sendEvent(w, False, 0, (XEvent *) &ev);
    }

    std::string WindowObserver::getCurrentWindowClass() {
        return global_grabber->current_class->get();
    }
}
