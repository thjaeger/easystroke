#include "eventloop.h"

#include "events/handler.h"
#include "log.h"
#include "xserverproxy.h"
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <sstream>
#include <iomanip>
#include <utility>
#include "trace.h"

EventLoop *global_eventLoop = nullptr;

bool EventLoop::idle() {
    return !handler->child;
}

void EventLoop::queue(std::function<void()> f) {
    if (idle()) {
        f();
        global_xServer->flush();
    } else {
        queued.push_back(f);
    }
}

void EventLoop::processQueue() {
    while (!this->queued.empty() && this->idle()) {
        this->queued.front()();
        this->queued.pop_front();
    }
}

void EventLoop::handle_event(XEvent &ev) {
    if (this->windowObserver->handle(ev)) {
        return;
    }

    switch (ev.type) {
        case ButtonPress:
            g_debug("Press (master): %d (%d, %d) at t = %ld", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y,
                    ev.xbutton.time);
            handler->top()->press_master(ev.xbutton.button, ev.xbutton.time);
            return;

        case MappingNotify:
            if (ev.xmapping.request == MappingPointer)
                update_core_mapping();
            if (ev.xmapping.request == MappingKeyboard || ev.xmapping.request == MappingModifier)
                XRefreshKeyboardMapping(&ev.xmapping);
            return;

        case GenericEvent:
            if (ev.xcookie.extension == this->grabber->devices->opcode && global_xServer->getEventData(&ev.xcookie)) {
                handle_xi2_event((XIDeviceEvent *) ev.xcookie.data);
                global_xServer->freeEventData(&ev.xcookie);
            }
    }
}

static std::string  render_coordinates(XIValuatorState *valuators, double *values) {
    std::ostringstream ss;
    ss << std::setprecision(3) << std::fixed;

    int n = 0;
    for (int i = valuators->mask_len - 1; i >= 0; i--) {
        if (XIMaskIsSet(valuators->mask, i)) {
            n = i + 1;
            break;
        }
    }
    bool first = true;
    int elt = 0;
    for (int i = 0; i < n; i++) {
        if (first)
            first = false;
        else
            ss << ", ";
        if (XIMaskIsSet(valuators->mask, i))
            ss << values[elt++];
        else
            ss << "*";
    }

    return ss.str();
}

void EventLoop::report_xi2_event(XIDeviceEvent *event, const char *type) {
    if (event->detail) {
        g_debug("%s (XI2): %d (%.3f, %.3f) - (%s) at t = %ld", type, event->detail, event->root_x, event->root_y,
                render_coordinates(&event->valuators, event->valuators.values).c_str(), event->time);
    } else {
        g_debug("%s (XI2): (%.3f, %.3f) - (%s) at t = %ld", type, event->root_x, event->root_y,
                render_coordinates(&event->valuators, event->valuators.values).c_str(), event->time);
    }
}

void EventLoop::handle_xi2_event(XIDeviceEvent *event) {
    switch (event->evtype) {
        case XI_ButtonPress:
            if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG))
                report_xi2_event(event, "Press");
            if (!xinput_pressed.empty()) {
                if (!current_dev || current_dev->dev != event->deviceid)
                    break;
            } else {
                this->windowObserver->setCurrentWindow(event->child);
            }
            current_dev = this->grabber->devices->get_xi_dev(event->deviceid);
            if (!current_dev) {
                g_warning("Warning: Spurious device event");
                break;
            }
            if (current_dev->master)
                global_xServer->setClientPointer(None, current_dev->master);
            if (xinput_pressed.empty()) {
                auto default_mods = this->grabber->buttonGrabber->get_default_mods(event->detail);
                if (default_mods == AnyModifier || default_mods == (guint) event->mods.base)
                    modifiers = AnyModifier;
                else
                    modifiers = event->mods.base;
            }
            xinput_pressed.insert(event->detail);
            in_proximity = current_dev->inProximity(event->valuators);
            handler->top()->press(event->detail, CursorPosition(event->root_x, event->root_y, event->time));
            break;
        case XI_ButtonRelease:
            if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG))
                report_xi2_event(event, "Release");
            if (!current_dev || current_dev->dev != event->deviceid)
                break;
            xinput_pressed.erase(event->detail);
            in_proximity = current_dev->inProximity(event->valuators);
            handler->top()->release(event->detail, CursorPosition(event->root_x, event->root_y, event->time));
            break;
        case XI_Motion:
            if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG))
                report_xi2_event(event, "Motion");
            if (!current_dev || current_dev->dev != event->deviceid)
                break;
            handler->top()->motion(CursorPosition(event->root_x, event->root_y, event->time));
            break;
        case XI_RawMotion:
            in_proximity = current_dev->inProximity(((XIRawEvent *) event)->valuators);
            handle_raw_motion((XIRawEvent *) event);
            break;
        case XI_HierarchyChanged:
            this->grabber->devices->hierarchy_changed((XIHierarchyEvent *) event);
    }
}

void EventLoop::handle_raw_motion(XIRawEvent *event) {
    if (!current_dev || current_dev->dev != event->deviceid)
        return;
    double x = 0.0, y = 0.0;
    bool abs_x = current_dev->absolute;
    bool abs_y = current_dev->absolute;
    int i = 0;

    if (XIMaskIsSet(event->valuators.mask, 0))
        x = event->raw_values[i++];
    else
        abs_x = false;

    if (XIMaskIsSet(event->valuators.mask, 1))
        y = event->raw_values[i++];
    else
        abs_y = false;

    if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG)) {
        g_debug("Raw motion (XI2): (%s) at t = %ld", render_coordinates(&event->valuators, event->raw_values).c_str(),
                event->time);
    }

    handler->top()->raw_motion(CursorPosition(x * current_dev->scale_x, y * current_dev->scale_y, event->time), abs_x,
                               abs_y);
}

bool EventLoop::handle(Glib::IOCondition) {
    while (this->xServer->countPendingEvents()) {
        try {
            XEvent ev;
            this->xServer->nextEvent(&ev);
            handle_event(ev);
        } catch (Events::GrabFailedException &e) {
            g_error("%s", e.what());
        }
    }
    return true;
}

void EventLoop::update_core_mapping() {
    unsigned char map[MAX_BUTTONS];
    int n = global_xServer->getPointerMapping(map, MAX_BUTTONS);
    core_inv_map.clear();
    for (int i = n - 1; i; i--)
        if (map[i] == i + 1)
            core_inv_map.erase(i + 1);
        else
            core_inv_map[map[i]] = i + 1;
}

void EventLoop::fake_core_button(guint b, bool press) {
    if (core_inv_map.count(b))
        b = core_inv_map[b];
    global_xServer->fakeButtonEvent(b, press, CurrentTime);
}

void EventLoop::fake_click(guint b) {
    fake_core_button(b, true);
    fake_core_button(b, false);
}

int EventLoop::xErrorHandler(Display *dpy2, XErrorEvent *e) {
    if (!global_xServer->isSameDisplayAs(dpy2))
        return global_eventLoop->oldHandler(dpy2, e);
    if (e->error_code == BadWindow) {
        switch (e->request_code) {
            case X_ChangeWindowAttributes:
            case X_GetProperty:
            case X_QueryTree:
                return 0;
        }
    }

    char text[64];
    global_xServer->getErrorText(e->error_code, text, sizeof text);
    char msg[16];
    snprintf(msg, sizeof msg, "%d", e->request_code);
    char def[128];
    if (e->request_code < 128) {
        snprintf(def, sizeof def, "request_code=%d, minor_code=%d", e->request_code, e->minor_code);
    } else {
        snprintf(def, sizeof def, "extension=%s, request_code=%d", global_eventLoop->opcodes[e->request_code].c_str(),
                 e->minor_code);
    }
    char dbtext[128];
    global_xServer->getErrorDatabaseText("XRequest", msg, def, dbtext, sizeof dbtext);
    g_warning("XError: %s: %s", text, dbtext);

    return 0;
}

int EventLoop::xIOErrorHandler(Display *dpy2) {
    if (!global_xServer->isSameDisplayAs(dpy2))
        return global_eventLoop->oldIOHandler(dpy2);
    g_error("Connection to X server lost");
}

void EventLoop::remove_device(int deviceid) {
    if (current_dev && current_dev->dev == deviceid) {
        current_dev = nullptr;
    }
}

void EventLoop::ungrab(int deviceid) {
    if (current_dev && current_dev->dev == deviceid) {
        xinput_pressed.clear();
    }
}

class ReloadTrace : public Timeout {
    void timeout() override {
        g_debug("Reloading gesture display");
        global_eventLoop->queue([]() { resetTrace(); });
    }
} reload_trace;

static void schedule_reload_trace() { reload_trace.set_timeout(1000); }

EventLoop::EventLoop(std::shared_ptr<XServerProxy> xServer)
        : xServer(std::move(xServer)),
          current_dev(nullptr),
          in_proximity(false),
          modifiers(0) {
    int n, opcode, event, error;
    char **ext = global_xServer->listExtensions(&n);
    for (int i = 0; i < n; i++) {
        if (global_xServer->queryExtension(ext[i], &opcode, &event, &error)) {
            opcodes[opcode] = ext[i];
        }
    }

    XServerProxy::freeExtensionList(ext);
    oldHandler = XSetErrorHandler(xErrorHandler);
    oldIOHandler = XSetIOErrorHandler(xIOErrorHandler);
    handler = HandlerFactory::makeIdleHandler(this);
    handler->init();
    this->windowObserver = std::make_unique<Events::WindowObserver>(this->xServer->ROOT);
    this->grabber = std::make_shared<Grabber>();

    // Force enter events to be generated
    this->xServer->grabPointer(this->xServer->ROOT, False, 0, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    this->xServer->ungrabPointer(CurrentTime);

    resetTrace();

    Glib::RefPtr<Gdk::Screen> screen = Gdk::Display::get_default()->get_default_screen();
    g_signal_connect(screen->gobj(), "composited-changed", &schedule_reload_trace, nullptr);
    screen->signal_size_changed().connect(sigc::ptr_fun(&schedule_reload_trace));

    this->xServer->grabControl(True);

    Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(this->xServer->getConnectionNumber(), Glib::IO_IN);
    io->connect(sigc::mem_fun(*global_eventLoop, &EventLoop::handle));
    io->attach();
}
