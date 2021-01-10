#include "windowobserver.h"

#include "eventloop.h"
#include "grabber.h"
#include "xserverproxy.h"

namespace Events {
    Window getAppWindow(Window w) {
        return grabbers::get_app_window(w);
    }

    std::string get_wm_class(Window w) {
        return global_xServer->getClassHint(w)->windowClass;
    }

    class ApplicationWindow {
    public:
        explicit ApplicationWindow(Window window) : window(window), classHint(global_xServer->getClassHint(window)) {}

        Window window;
        std::unique_ptr<WindowClassHint> classHint;
    };

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
            : currentAppWindow(Source<Window>(None)),
              currentApplicationWindow(std::make_unique<ApplicationWindow>(None)) {
        current_class = fun(&get_wm_class, this->currentAppWindow);
        current_class->connect(new IdleNotifier());
    }

    WindowObserver::~WindowObserver() = default;

    void WindowObserver::handleEnterLeave(XEvent &ev) {
        if (ev.xcrossing.mode == NotifyGrab) {
            return;
        }

        if (ev.xcrossing.detail == NotifyInferior) {
            return;
        }

        if (ev.type == EnterNotify) {
            this->setCurrentWindow(ev.xcrossing.window);
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
        if (!this->currentApplicationWindow) {
            return "";
        }

        return this->currentApplicationWindow->classHint->windowClass;
    }

    void WindowObserver::setCurrentWindow(Window newWindow) {
        auto newAppWindow = getAppWindow(newWindow);
        if (newAppWindow == currentAppWindow.get()) {
            return;
        }

        currentAppWindow.set(newAppWindow);
        g_debug("Changed current window 0x%lx -> 0x%lx", newWindow, currentAppWindow.get());
    }
}
