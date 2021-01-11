#include "windowobserver.h"

#include "eventloop.h"
#include "grabber.h"
#include "xserverproxy.h"

namespace Events {
    template<class X1, class X2>
    class BiMap {
        std::map<X1, X2> map1;
        std::map<X2, X1> map2;
    public:
        void erase1(X1 x1) {
            auto i1 = map1.find(x1);
            if (i1 == map1.end())
                return;
            map2.erase(i1->second);
            map1.erase(i1->first);
        }

        void erase2(X2 x2) {
            auto i2 = map2.find(x2);
            if (i2 == map2.end())
                return;
            map1.erase(i2->second);
            map2.erase(i2->first);
        }

        void add(X1 x1, X2 x2) {
            erase1(x1);
            erase2(x2);
            map1[x1] = x2;
            map2[x2] = x1;
        }

        bool contains1(X1 x1) { return map1.find(x1) != map1.end(); }

        X2 find1(X1 x1) { return map1.find(x1)->second; }
    };

    BiMap<Window, Window> frame_win;
    BiMap<Window, Window> frame_child;

    std::list<Window> minimized;

    void get_frame(Window w) {
        Window frame = global_xServer->getWindow(w, global_xServer->atoms.NET_FRAME_WINDOW);
        if (!frame)
            return;
        frame_win.add(frame, w);
    }

    static bool has_wm_state(Window w) {
        Atom actual_type_return;
        int actual_format_return;
        unsigned long nitems_return;
        unsigned long bytes_after_return;
        unsigned char *prop_return;
        if (Success != global_xServer->getWindowProperty(
                w, global_xServer->atoms.WM_STATE, 0, 2, False,
                AnyPropertyType, &actual_type_return,
                &actual_format_return, &nitems_return,
                &bytes_after_return, &prop_return))
            return false;
        XServerProxy::free(prop_return);
        return nitems_return;
    }

    Window find_wm_state(Window w) {
        if (!w)
            return w;
        if (has_wm_state(w))
            return w;
        Window found = None;
        unsigned int n;
        Window dummyw1, dummyw2, *ch;
        if (!global_xServer->queryTree(w, &dummyw1, &dummyw2, &ch, &n))
            return None;
        for (unsigned int i = 0; i != n; i++)
            if (has_wm_state(ch[i]))
                found = ch[i];
        if (!found)
            for (unsigned int i = 0; i != n; i++) {
                found = find_wm_state(ch[i]);
                if (found)
                    break;
            }
        XServerProxy::free(ch);
        return found;
    }

    Window getAppWindow(Window w) {
        if (!w)
            return w;

        if (frame_win.contains1(w))
            return frame_win.find1(w);

        if (frame_child.contains1(w))
            return frame_child.find1(w);

        Window w2 = find_wm_state(w);
        if (w2) {
            frame_child.add(w, w2);
            if (w2 != w) {
                w = w2;
                global_xServer->selectInput(w2, StructureNotifyMask | PropertyChangeMask);
            }
            return w2;
        }
        g_message("Window 0x%lx does not have an associated top-level window", w);
        return w;
    }

    class BasicApplicationInfo {
    public:
        explicit BasicApplicationInfo(Window window)
                : window(window),
                  classHint(global_xServer->getClassHint(window)) {
        }

        Window window;
        std::unique_ptr<WindowClassHint> classHint;

        [[nodiscard]] bool isEqualTo(const BasicApplicationInfo &other) const {
            return this->window == other.window &&
                   this->classHint->windowClass == other.classHint->windowClass &&
                   this->classHint->windowName == other.classHint->windowName;
        }
    };

    WindowObserver::WindowObserver(Window window)
            : currentApplication(std::make_unique<BasicApplicationInfo>(None)), parent(window) {
        global_xServer->selectInput(parent, SubstructureNotifyMask);
        unsigned int n;
        Window dummyw1, dummyw2, *ch;
        global_xServer->queryTree(parent, &dummyw1, &dummyw2, &ch, &n);
        for (unsigned int i = 0; i < n; i++) {
            add(ch[i]);
        }
        XServerProxy::free(ch);
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
            g_debug("Changed current window from %s to %s", previousWindowName.c_str(),
                    currentApplication->classHint->windowClass.c_str());
        }
    }

    bool WindowObserver::handle(XEvent &ev) {
        switch (ev.type) {
            case EnterNotify:
            case LeaveNotify:
                this->handleEnterLeave(ev);
                return true;

            case CreateNotify:
                if (ev.xcreatewindow.parent != parent)
                    return false;
                add(ev.xcreatewindow.window);
                return true;
            case DestroyNotify:
                frame_child.erase1(ev.xdestroywindow.window);
                frame_child.erase2(ev.xdestroywindow.window);
                minimized.remove(ev.xdestroywindow.window);
                destroy(ev.xdestroywindow.window);
                return true;
            case ReparentNotify:
                if (ev.xreparent.event != parent)
                    return false;
                if (ev.xreparent.window == parent)
                    return false;
                if (ev.xreparent.parent == parent)
                    add(ev.xreparent.window);
                else
                    remove(ev.xreparent.window);
                return true;
            case PropertyNotify:
                if (ev.xproperty.atom == global_xServer->atoms.NET_FRAME_WINDOW) {
                    if (ev.xproperty.state == PropertyDelete)
                        frame_win.erase1(ev.xproperty.window);
                    if (ev.xproperty.state == PropertyNewValue)
                        get_frame(ev.xproperty.window);
                    return true;
                }
                if (ev.xproperty.atom == global_xServer->atoms.NET_WM_STATE) {
                    if (ev.xproperty.state == PropertyDelete) {
                        minimized.remove(ev.xproperty.window);
                        return true;
                    }
                    bool was_hidden =
                            std::find(minimized.begin(), minimized.end(), ev.xproperty.window) != minimized.end();
                    bool is_hidden = global_xServer->hasAtom(ev.xproperty.window, global_xServer->atoms.NET_WM_STATE,
                                                             global_xServer->atoms.NET_WM_STATE_HIDDEN);
                    if (was_hidden && !is_hidden)
                        minimized.remove(ev.xproperty.window);
                    if (is_hidden && !was_hidden)
                        minimized.push_back(ev.xproperty.window);
                    return true;
                }

                this->handlePropertyNotify(ev);
                return true;
            default:
                return false;
        }
    }

    void WindowObserver::add(Window w) {
        if (!w) {
            return;
        }

        global_xServer->selectInput(w, EnterWindowMask | PropertyChangeMask);
        get_frame(w);
    }

    void WindowObserver::remove(Window w) {
        global_xServer->selectInput(w, 0);
        destroy(w);
    }

    void WindowObserver::destroy(Window w) {
        frame_win.erase1(w);
        frame_win.erase2(w);
    }
}
