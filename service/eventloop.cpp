#include "eventloop.h"

#include "handler.h"
#include "log.h"
#include "xserverproxy.h"
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include "actions/modAction.h"
#include <sstream>
#include <iomanip>

XState *xstate = nullptr;

extern Window get_app_window(Window w);
extern Source<Window> current_app_window;

bool XState::idle() {
    return !handler->child;
}

void XState::queue(sigc::slot<void> f) {
    if (idle()) {
        f();
        global_xServer->flush();
    } else {
        queued.push_back(f);
    }
}

void XState::handle_enter_leave(XEvent &ev) {
    if (ev.xcrossing.mode == NotifyGrab)
        return;
    if (ev.xcrossing.detail == NotifyInferior)
        return;
    Window w = ev.xcrossing.window;
    if (ev.type == EnterNotify) {
        current_app_window.set(get_app_window(w));
        g_debug("Entered window 0x%lx -> 0x%lx", w, current_app_window.get());
    } else {
        g_warning("Error: Bogus Enter/Leave event");
    };
}

#define H (handler->top())
void XState::handle_event(XEvent &ev) {

    switch(ev.type) {
        case EnterNotify:
        case LeaveNotify:
            handle_enter_leave(ev);
            return;

        case PropertyNotify:
            if (current_app_window.get() == ev.xproperty.window && ev.xproperty.atom == XA_WM_CLASS)
                current_app_window.notify();
            return;

        case ButtonPress:
            g_debug("Press (master): %d (%d, %d) at t = %ld", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y,
                    ev.xbutton.time);
            H->press_master(ev.xbutton.button, ev.xbutton.time);
            return;

        case ClientMessage:
            if (ev.xclient.window != ping_window)
                return;
            return;

        case MappingNotify:
            if (ev.xmapping.request == MappingPointer)
                update_core_mapping();
            if (ev.xmapping.request == MappingKeyboard || ev.xmapping.request == MappingModifier)
                XRefreshKeyboardMapping(&ev.xmapping);
            return;
        case GenericEvent:
            if (ev.xcookie.extension == grabber->opcode && global_xServer->getEventData(&ev.xcookie)) {
                handle_xi2_event((XIDeviceEvent *) ev.xcookie.data);
                global_xServer->freeEventData(&ev.xcookie);
            }
    }
}

void XState::activate_window(Window w, Time t) {
    if (w == global_xServer->getWindow(global_xServer->ROOT, global_xServer->atoms.NET_ACTIVE_WINDOW))
        return;

    auto window_type = global_xServer->getAtom(w, global_xServer->atoms.NET_WM_WINDOW_TYPE);
    if (window_type == global_xServer->atoms.NET_WM_WINDOW_TYPE_DOCK)
        return;

    auto *wm_hints = global_xServer->getWMHints(w);
    if (wm_hints) {
        bool input = wm_hints->input;
        XServerProxy::free(wm_hints);
        if (!input)
            return;
    }

    if (!global_xServer->hasAtom(w, global_xServer->atoms.WM_PROTOCOLS, global_xServer->atoms.WM_TAKE_FOCUS))
        return;

    XWindowAttributes attr;
    if (global_xServer->getWindowAttributes(w, &attr) && attr.override_redirect) {
        return;
    }

    g_debug("Giving focus to window 0x%lx", w);

    icccm_client_message(w, global_xServer->atoms.WM_TAKE_FOCUS, t);
}

void XState::icccm_client_message(Window w, Atom a, Time t) {
    XClientMessageEvent ev;
    ev.type = ClientMessage;
    ev.window = w;
    ev.message_type = global_xServer->atoms.WM_PROTOCOLS;
    ev.format = 32;
    ev.data.l[0] = a;
    ev.data.l[1] = t;
    global_xServer->sendEvent(w, False, 0, (XEvent *) &ev);
}


static std::string render_coordinates(XIValuatorState *valuators, double *values) {
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
            ss <<"*";
    }

    return ss.str();
}

static double get_axis(XIValuatorState &valuators, int axis) {
    if (axis < 0 || !XIMaskIsSet(valuators.mask, axis))
        return 0.0;
    double *val = valuators.values;
    for (int i = 0; i < axis; i++)
        if (XIMaskIsSet(valuators.mask, i))
            val++;
    return *val;
}

void XState::report_xi2_event(XIDeviceEvent *event, const char *type) {
    if (event->detail) {
        g_debug("%s (XI2): %d (%.3f, %.3f) - (%s) at t = %ld", type, event->detail, event->root_x, event->root_y,
                render_coordinates(&event->valuators, event->valuators.values).c_str(), event->time);
    } else {
        g_debug("%s (XI2): (%.3f, %.3f) - (%s) at t = %ld", type, event->root_x, event->root_y,
                render_coordinates(&event->valuators, event->valuators.values).c_str(), event->time);
    }
}

void XState::handle_xi2_event(XIDeviceEvent *event) {
    switch (event->evtype) {
        case XI_ButtonPress:
            if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG))
                report_xi2_event(event, "Press");
            if (!xinput_pressed.empty()) {
                if (!current_dev || current_dev->dev != event->deviceid)
                    break;
            } else {
                current_app_window.set(get_app_window(event->child));
                g_debug("Active window 0x%lx -> 0x%lx", event->child, current_app_window.get());
            }
            current_dev = grabber->get_xi_dev(event->deviceid);
            if (!current_dev) {
                g_warning("Warning: Spurious device event");
                break;
            }
            if (current_dev->master)
                global_xServer->setClientPointer(None, current_dev->master);
            if (xinput_pressed.empty()) {
                guint default_mods = grabber->get_default_mods(event->detail);
                if (default_mods == AnyModifier || default_mods == (guint)event->mods.base)
                    modifiers = AnyModifier;
                else
                    modifiers = event->mods.base;
            }
            xinput_pressed.insert(event->detail);
            in_proximity = get_axis(event->valuators, current_dev->proximity_axis);
            H->press(event->detail, CursorPosition(event->root_x, event->root_y, event->time));
            break;
        case XI_ButtonRelease:
            if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG))
                report_xi2_event(event, "Release");
            if (!current_dev || current_dev->dev != event->deviceid)
                break;
            xinput_pressed.erase(event->detail);
            in_proximity = get_axis(event->valuators, current_dev->proximity_axis);
            H->release(event->detail, CursorPosition(event->root_x, event->root_y, event->time));
            break;
        case XI_Motion:
            if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG))
                report_xi2_event(event, "Motion");
            if (!current_dev || current_dev->dev != event->deviceid)
                break;
            H->motion(CursorPosition(event->root_x, event->root_y, event->time));
            break;
        case XI_RawMotion:
            in_proximity = get_axis(((XIRawEvent *)event)->valuators, current_dev->proximity_axis);
            handle_raw_motion((XIRawEvent *)event);
            break;
        case XI_HierarchyChanged:
            grabber->hierarchy_changed((XIHierarchyEvent *)event);
    }
}

void XState::handle_raw_motion(XIRawEvent *event) {
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
        g_debug("Raw motion (XI2): (%s) at t = %ld", render_coordinates(&event->valuators, event->raw_values).c_str(), event->time);
    }

    H->raw_motion(CursorPosition(x * current_dev->scale_x, y * current_dev->scale_y, event->time), abs_x, abs_y);
}

#undef H

bool XState::handle(Glib::IOCondition) {
    while (global_xServer->countPendingEvents()) {
        try {
            XEvent ev;
            global_xServer->nextEvent(&ev);
            if (!grabber->handle(ev))
                handle_event(ev);
        } catch (GrabFailedException &e) {
            g_error("%s", e.what());
        }
    }
    return true;
}

void XState::update_core_mapping() {
    unsigned char map[MAX_BUTTONS];
    int n = global_xServer->getPointerMapping(map, MAX_BUTTONS);
    core_inv_map.clear();
    for (int i = n - 1; i; i--)
        if (map[i] == i + 1)
            core_inv_map.erase(i + 1);
        else
            core_inv_map[map[i]] = i + 1;
}

void XState::fake_core_button(guint b, bool press) {
    if (core_inv_map.count(b))
        b = core_inv_map[b];
    global_xServer->fakeButtonEvent(b, press, CurrentTime);
}

void XState::fake_click(guint b) {
    fake_core_button(b, true);
    fake_core_button(b, false);
}

int XState::xErrorHandler(Display *dpy2, XErrorEvent *e) {
    if (!global_xServer->isSameDisplayAs(dpy2))
        return xstate->oldHandler(dpy2, e);
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
    if (e->request_code < 128)
        snprintf(def, sizeof def, "request_code=%d, minor_code=%d", e->request_code, e->minor_code);
    else
        snprintf(def, sizeof def, "extension=%s, request_code=%d", xstate->opcodes[e->request_code].c_str(), e->minor_code);
    char dbtext[128];
    global_xServer->getErrorDatabaseText("XRequest", msg, def, dbtext, sizeof dbtext);
    g_warning("XError: %s: %s", text, dbtext);

    return 0;
}

int XState::xIOErrorHandler(Display *dpy2) {
    if (!global_xServer->isSameDisplayAs(dpy2))
        return xstate->oldIOHandler(dpy2);
    g_error("Connection to X server lost");
}

void XState::remove_device(int deviceid) {
    if (current_dev && current_dev->dev == deviceid)
        current_dev = nullptr;
}

void XState::ungrab(int deviceid) {
    if (current_dev && current_dev->dev == deviceid)
        xinput_pressed.clear();
}

XState::XState() : current_dev(nullptr), in_proximity(false), accepted(true), modifiers(0) {
    int n, opcode, event, error;
    char **ext = global_xServer->listExtensions(&n);
    for (int i = 0; i < n; i++)
        if (global_xServer->queryExtension(ext[i], &opcode, &event, &error))
            opcodes[opcode] = ext[i];
    XServerProxy::freeExtensionList(ext);
    oldHandler = XSetErrorHandler(xErrorHandler);
    oldIOHandler = XSetIOErrorHandler(xIOErrorHandler);
    ping_window = global_xServer->createSimpleWindow(global_xServer->ROOT, 0, 0, 1, 1, 0, 0, 0);
    handler = HandlerFactory::makeIdleHandler(this);
    handler->init();
}
