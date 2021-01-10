#include "windowobserver.h"

#include "eventloop.h"
#include "grabber.h"
#include "xserverproxy.h"

namespace Events {
    Window getAppWindow(Window w) {
        return grabbers::get_app_window(w);
    }

    std::string get_wm_class(Window w) {
        if (!w)
            return "";
        XClassHint ch;
        if (!global_xServer->getClassHint(w, &ch)) {
            return "";
        }

        std::string ans = ch.res_name;
        XServerProxy::free(ch.res_name);
        XServerProxy::free(ch.res_class);
        return ans;
    }

    class IdleNotifier : public Base {
        void run() {
            global_grabber->update();
        }

    public:
        void notify() override {
            global_eventLoop->queue(sigc::mem_fun(*this, &IdleNotifier::run));
        }
    };

    WindowObserver::WindowObserver()
            : currentAppWindow(Source<Window>(None)) {
        current_class = fun(&get_wm_class, this->currentAppWindow);
        current_class->connect(new IdleNotifier());
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
            currentAppWindow.set(getAppWindow(w));
            g_debug("Entered window 0x%lx -> 0x%lx", w, currentAppWindow.get());
        } else {
            g_warning("Error: Bogus Enter/Leave event");
        };
    }

    void WindowObserver::handlePropertyNotify(XEvent &ev) {
        if (currentAppWindow.get() == ev.xproperty.window &&
            ev.xproperty.atom == XA_WM_CLASS) {
            currentAppWindow.notify();
        }
    }

    void WindowObserver::tryActivateCurrentWindow(Time t) {
        if (!currentAppWindow.get()) {
            return;
        }

        auto w = currentAppWindow.get();
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
        return this->current_class->get();
    }

    void WindowObserver::setCurrentWindow(Window newWindow) {
        currentAppWindow.set(getAppWindow(newWindow));
        g_debug("Active window 0x%lx -> 0x%lx", newWindow, currentAppWindow.get());
    }
}
