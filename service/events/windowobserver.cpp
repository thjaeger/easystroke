#include "windowobserver.h"

#include "eventloop.h"
#include "grabber.h"
#include "xserverproxy.h"

namespace Events {
    Window getAppWindow(Window w) {
        return grabbers::get_app_window(w);
    }

    class BasicApplicationInfo {
    public:
        explicit BasicApplicationInfo(Window window) : window(window),
                                                       classHint(global_xServer->getClassHint(window)) {}

        Window window;
        std::unique_ptr<WindowClassHint> classHint;

        [[nodiscard]] bool isEqualTo(const BasicApplicationInfo& other) const {
            return this->window == other.window &&
            this->classHint->windowClass == other.classHint->windowClass &&
                   this->classHint->windowName == other.classHint->windowName;
        }
    };

    WindowObserver::WindowObserver()
            : currentApplication(std::make_unique<BasicApplicationInfo>(None)) {
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
        if (currentApplication->window == ev.xproperty.window &&
            ev.xproperty.atom == XA_WM_CLASS) {
            setCurrentApplication(ev.xproperty.window);
        }
    }

    void WindowObserver::tryActivateCurrentWindow(Time t) {
        auto w = currentApplication->window;
        if (!w) {
            return;
        }

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
        return this->currentApplication->classHint->windowClass;
    }

    void WindowObserver::setCurrentWindow(Window newWindow) {
        auto newAppWindow = getAppWindow(newWindow);
        if (newAppWindow == currentApplication->window) {
            return;
        }

        setCurrentApplication(newAppWindow);
    }

    void WindowObserver::setCurrentApplication(Window applicationWindow) {
        auto newApplication = std::make_unique<BasicApplicationInfo>(applicationWindow);

        if (!newApplication->isEqualTo(*currentApplication)) {
            auto previousWindowName = currentApplication->classHint->windowClass;

            currentApplication = std::move(newApplication);
            global_eventLoop->queue([]() { global_eventLoop->grabber->update(); });
            g_debug("Changed current window from %s to %s", previousWindowName.c_str(), currentApplication->classHint->windowClass.c_str());
        }
    }
}
