#include "xserverproxy.h"
#include <xorg/xserver-properties.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>

std::shared_ptr<XServerProxy> global_xServer;

Atom internAtom(Display *dpy, const std::string &name) {
    return XInternAtom(dpy, name.c_str(), false);
}

XAtoms::XAtoms(Display *dpy) :
        NET_FRAME_WINDOW(internAtom(dpy, "_NET_FRAME_WINDOW")),
        NET_WM_STATE(internAtom(dpy, "_NET_WM_STATE")),
        NET_WM_STATE_HIDDEN(internAtom(dpy, "_NET_WM_STATE_HIDDEN")),
        NET_ACTIVE_WINDOW(internAtom(dpy, "_NET_ACTIVE_WINDOW")),
        NET_WM_WINDOW_TYPE(internAtom(dpy, "_NET_WM_WINDOW_TYPE")),
        NET_WM_WINDOW_TYPE_DOCK(internAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK")),
        WM_PROTOCOLS(internAtom(dpy, "WM_PROTOCOLS")),
        WM_TAKE_FOCUS(internAtom(dpy, "WM_TAKE_FOCUS")),
        XTEST(internAtom(dpy, XI_PROP_XTEST_DEVICE)),
        PROXIMITY(internAtom(dpy, AXIS_LABEL_PROP_ABS_DISTANCE)),
        WM_STATE(internAtom(dpy, "WM_STATE"))
{
}

std::shared_ptr<XServerProxy> XServerProxy::Open() {
    if (global_xServer) {
        g_error("Global xServer already configured");
    }


    auto dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        g_error("Couldn't open display.");
    }

    auto atoms = XAtoms(dpy);
    auto result = std::make_shared<XServerProxy>(dpy, atoms);
    global_xServer = result;
    return result;
}

XServerProxy::XServerProxy(Display *dpy, XAtoms atoms) : dpy(dpy), ROOT(DefaultRootWindow(dpy)), atoms(atoms) {
}

XServerProxy::~XServerProxy() {
    XCloseDisplay(this->dpy);
}

int XServerProxy::countPendingEvents() const {
    return XPending(this->dpy);
}

Cursor XServerProxy::createFontCursor(unsigned int shape) const {
    return XCreateFontCursor(this->dpy, shape);
}

Window XServerProxy::createSimpleWindow(Window parent, int x, int y, unsigned int width, unsigned int height,
                                        unsigned int border_width, unsigned long border, unsigned long background) const {
    return XCreateSimpleWindow(this->dpy, parent, x, y, width, height, border_width, border, background);
}

int XServerProxy::fakeButtonEvent(unsigned int button, int is_press, unsigned long delay) const {
    return XTestFakeButtonEvent(this->dpy, button, is_press, delay);
}

int XServerProxy::fakeKeyEvent(unsigned int keycode, int is_press, unsigned long delay) const {
    return XTestFakeKeyEvent(this->dpy, keycode, is_press, delay);
}

int XServerProxy::fakeMotionEvent(int screen, int x, int y, unsigned long delay) const {
    return XTestFakeMotionEvent(this->dpy, screen, x, y, delay);
}

int XServerProxy::flush() const {
    return XFlush(this->dpy);
}

void XServerProxy::free(void *data) {
    XFree(data);
}

int XServerProxy::freeCursor(Cursor cursor) const {
    return XFreeCursor(this->dpy, cursor);
}

void XServerProxy::freeEventData(XGenericEventCookie *cookie) const {
    XFreeEventData(this->dpy, cookie);
}

int XServerProxy::freeExtensionList(char **list) {
    return XFreeExtensionList(list);
}

int XServerProxy::freeModifiermap(XModifierKeymap *modmap) {
    return XFreeModifiermap(modmap);
}

Atom XServerProxy::getAtom(Window w, Atom prop) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_return = nullptr;

    if (global_xServer->getWindowProperty(w, prop, 0, sizeof(Atom), False, XA_ATOM, &actual_type, &actual_format,
                                          &nitems, &bytes_after, &prop_return) != Success)
        return None;
    if (!prop_return)
        return None;
    Atom atom = *(Atom *) prop_return;
    XServerProxy::free(prop_return);
    return atom;
}

Status XServerProxy::getClassHint(Window w, XClassHint *class_hints_return) const {
    return XGetClassHint(this->dpy, w, class_hints_return);
}

int XServerProxy::getConnectionNumber() {
    return ConnectionNumber(this->dpy);
}

int XServerProxy::getDefaultScreen() const {
    return DefaultScreen(this->dpy);
}

double XServerProxy::getDisplayHeight() {
    return (double) DisplayHeight(this->dpy, DefaultScreen(this->dpy));
}

double XServerProxy::getDisplayWidth() {
    return (double) DisplayWidth(this->dpy, DefaultScreen(this->dpy));
}

int XServerProxy::getErrorDatabaseText(const char *name, const char *message, const char *default_string, char *buffer_return,
                                       int length) const {
    return XGetErrorDatabaseText(this->dpy, name, message, default_string, buffer_return, length);
}

int XServerProxy::getErrorText(int code, char *buffer_return, int length) const {
    return XGetErrorText(this->dpy, code, buffer_return, length);
}

Bool XServerProxy::getEventData(XGenericEventCookie *cookie) const {
    return XGetEventData(this->dpy, cookie);
}

Status XServerProxy::getInterfaceProperty(int deviceid, Atom property, long offset, long length,
                                          int delete_property, Atom type, Atom *type_return, int *format_return,
                                          unsigned long *num_items_return, unsigned long *bytes_after_return,
                                          unsigned char **data) const {
    return XIGetProperty(this->dpy, deviceid, property, offset, length, delete_property, type, type_return,
                         format_return, num_items_return, bytes_after_return, data);
}

KeySym *XServerProxy::getKeyboardMapping(
#if NeedWidePrototypes
        unsigned int	first_keycode
#else
        KeyCode first_keycode,
#endif
        int keycode_count,
        int *keysyms_per_keycode_return
) const {
    return XGetKeyboardMapping(this->dpy, first_keycode, keycode_count, keysyms_per_keycode_return);
}

XModifierKeymap *XServerProxy::getModifierMapping() const {
    return XGetModifierMapping(this->dpy);
}

int XServerProxy::getPointerMapping(unsigned char *map_return, int nmap) const {
    return XGetPointerMapping(this->dpy, map_return, nmap);
}

Window XServerProxy::getWindow(Window w, Atom prop) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_return = nullptr;

    if (this->getWindowProperty(
            w, prop, 0, sizeof(Atom), False, XA_WINDOW, &actual_type, &actual_format,
            &nitems, &bytes_after, &prop_return) != Success)
        return None;
    if (!prop_return)
        return None;
    Window ret = *(Window *) prop_return;
    XServerProxy::free(prop_return);
    return ret;
}

Status XServerProxy::getWindowAttributes(Window w, XWindowAttributes *window_attributes_return) const {
    return XGetWindowAttributes(this->dpy, w, window_attributes_return);
}

int XServerProxy::getWindowProperty(Window w, Atom property, long long_offset, long long_length, int deleteProperty,
                                    Atom req_type, Atom *actual_type_return, int *actual_format_return,
                                    unsigned long *nitems_return, unsigned long *bytes_after_return,
                                    unsigned char **prop_return) const {
    return XGetWindowProperty(this->dpy, w, property, long_offset, long_length, deleteProperty, req_type,
                              actual_type_return, actual_format_return, nitems_return, bytes_after_return, prop_return);
}

XWMHints *XServerProxy::getWMHints(Window w) const {
    return XGetWMHints(this->dpy, w);
}

int XServerProxy::grabControl(Bool impervious) const {
    return XTestGrabControl(this->dpy, impervious);
}

Status XServerProxy::grabDevice(
        int deviceid, Window grab_window, Time time, Cursor cursor, int grab_mode, int paired_device_mode,
        int owner_events, XIEventMask *mask) const {
    return XIGrabDevice(this->dpy, deviceid, grab_window, time, cursor, grab_mode, paired_device_mode, owner_events,
                        mask);
}

int XServerProxy::grabInterfaceButton(int deviceId, int button, Window grab_window, Cursor cursor, int grab_mode,
                                      int paired_device_mode, int owner_events, XIEventMask *mask, int num_modifiers,
                                      XIGrabModifiers *modifiers_inout) const {
    return XIGrabButton(this->dpy, deviceId, button, grab_window, cursor, grab_mode, paired_device_mode, owner_events,
                        mask, num_modifiers, modifiers_inout);
}

int XServerProxy::grabPointer(
        Window grab_window, int owner_events, unsigned int event_mask, int pointer_mode, int keyboard_mode,
        Window confine_to, Cursor cursor, Time time) const {
    return XGrabPointer(this->dpy, grab_window, owner_events, event_mask, pointer_mode, keyboard_mode, confine_to,
                        cursor, time);
}

bool XServerProxy::hasAtom(Window w, Atom prop, Atom value) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_return = nullptr;

    if (global_xServer->getWindowProperty(w, prop, 0, sizeof(Atom), False, XA_ATOM, &actual_type, &actual_format,
                                          &nitems, &bytes_after, &prop_return) != Success)
        return None;
    if (!prop_return)
        return None;
    Atom *atoms = (Atom *) prop_return;
    bool ans = false;
    for (unsigned long i = 0; i < nitems; i++)
        if (atoms[i] == value)
            ans = true;
    XServerProxy::free(prop_return);
    return ans;
}

void XServerProxy::hideCursor(Window win) const {
    XFixesHideCursor(this->dpy, win);
}

bool XServerProxy::isSameDisplayAs(Display *dpy2) {
    return this->dpy == dpy2;
}

KeyCode XServerProxy::keysymToKeycode(KeySym keysym) const {
    return XKeysymToKeycode(this->dpy, keysym);
}

char **XServerProxy::listExtensions(int *nextensions_return) const {
    return XListExtensions(this->dpy, nextensions_return);
}

int XServerProxy::nextEvent(XEvent *event_return) const {
    return XNextEvent(this->dpy, event_return);
}

XIDeviceInfo *XServerProxy::queryDevice(int deviceid, int *ndevices_return) const {
    return XIQueryDevice(this->dpy, deviceid, ndevices_return);
}

Bool XServerProxy::queryExtension(
        const char *name, int *major_opcode_return, int *first_event_return, int *first_error_return) const {
    return XQueryExtension(this->dpy, name, major_opcode_return, first_event_return, first_error_return);
}

Status XServerProxy::queryInterfaceVersion(int *major_version_inout, int *minor_version_inout) const {
    return XIQueryVersion(this->dpy, major_version_inout, minor_version_inout);
}

Status XServerProxy::queryTree(Window w, Window *root_return, Window *parent_return, Window **children_return,
                               unsigned int *nchildren_return) const {
    return XQueryTree(this->dpy, w, root_return, parent_return, children_return, nchildren_return);
}

Bool XServerProxy::queryPointer(Window w, Window *root_return, Window *child_return, int *root_x_return, int *root_y_return,
                                int *win_x_return, int *win_y_return, unsigned int *mask_return) const {
    return XQueryPointer(this->dpy, w, root_return, child_return, root_x_return, root_y_return, win_x_return,
                         win_y_return, mask_return);
}

Bool XServerProxy::ringBell(Window win, int percent, Atom name) const {
    return XkbBell(this->dpy, win, percent, name);
}

void XServerProxy::sendEvent(Window window, bool propagate, long event_mask, XEvent *event_send) const {
    XSendEvent(this->dpy, window, propagate, event_mask, event_send);
}

void XServerProxy::selectInput(Window window, long event_mask) const {
    XSelectInput(this->dpy, window, event_mask);
}

int XServerProxy::selectInterfaceEvents(Window win, XIEventMask *masks, int num_masks) const {
    return XISelectEvents(this->dpy, win, masks, num_masks);
}

Status XServerProxy::setClientPointer(Window win, int deviceid) const {
    return XISetClientPointer(this->dpy, win, deviceid);
}

void XServerProxy::showCursor(Window win) const {
    XFixesShowCursor(this->dpy, win);
}

Status XServerProxy::ungrabDevice(int deviceid, Time time) const {
    return XIUngrabDevice(this->dpy, deviceid, time);
}

Status XServerProxy::ungrabInterfaceButton(int deviceid, int button, Window grab_window, int num_modifiers,
                                           XIGrabModifiers *modifiers) const {
    return XIUngrabButton(this->dpy, deviceid, button, grab_window, num_modifiers, modifiers);
}

int XServerProxy::ungrabKey(int keycode, unsigned int modifiers, Window grab_window) const {
    return XUngrabKey(this->dpy, keycode, modifiers, grab_window);
}

int XServerProxy::ungrabPointer(Time time) const {
    return XUngrabPointer(this->dpy, time);
}
