#pragma once

#include <memory>
#include <gtkmm.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>

extern "C" {
    struct _XDisplay;
    typedef struct _XDisplay Display;
}

class XAtoms {
public:
    explicit XAtoms(Display* dpy);

    Atom NET_FRAME_WINDOW;
    Atom NET_WM_STATE;
    Atom NET_WM_STATE_HIDDEN;
    Atom NET_ACTIVE_WINDOW;
    Atom XTEST;
    Atom PROXIMITY;
    Atom WM_STATE;
    Atom NET_WM_WINDOW_TYPE;
    Atom NET_WM_WINDOW_TYPE_DOCK;
    Atom WM_PROTOCOLS;
    Atom WM_TAKE_FOCUS;
};

/**
 * This class is the once place that should know the inner details of XServer. It's intended to store the state of the
 * current X-Server and provide methods to interact with it (instead of importing X11 libraries everywhere). It's
 * essentially a simplified compatibility layer that can be passed around to avoid global state.
 */
class XServerProxy {
private:
    Display *dpy;

public:
    Window ROOT;

    static std::shared_ptr<XServerProxy> Open();

    XServerProxy(Display *dpy, XAtoms atoms);

    ~XServerProxy();

    XAtoms atoms;

    [[nodiscard]] int countPendingEvents() const;

    [[nodiscard]] Cursor createFontCursor(unsigned int shape) const;

    Window createSimpleWindow(
            Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width,
            unsigned long border, unsigned long background) const;

    int fakeButtonEvent(unsigned int button, Bool is_press, unsigned long delay) const;

    int fakeKeyEvent(unsigned int keycode, Bool is_press, unsigned long delay) const;

    int fakeMotionEvent(int screen, int x, int y, unsigned long delay) const;

    int flush() const;

    static void free(void *data);

#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-nodiscard"

    int freeCursor(Cursor cursor) const;

#pragma clang diagnostic pop

    void freeEventData(XGenericEventCookie *cookie) const;

    static int freeExtensionList(char **list);

    static int freeModifiermap(XModifierKeymap *modmap);

    Status getClassHint(Window w, XClassHint *class_hints_return) const;

    int getConnectionNumber();

    [[nodiscard]] int getDefaultScreen() const;

    [[nodiscard]] double getDisplayHeight();

    [[nodiscard]] double getDisplayWidth();

    int getErrorDatabaseText(
            _Xconst char *name, _Xconst char *message, _Xconst char *default_string, char *buffer_return,
            int length) const;

    int getErrorText(int code, char *buffer_return, int length) const;

    Bool getEventData(XGenericEventCookie *cookie) const;

    Status getInterfaceProperty(
            int deviceid,
            Atom property,
            long offset,
            long length,
            Bool delete_property,
            Atom type,
            Atom *type_return,
            int *format_return,
            unsigned long *num_items_return,
            unsigned long *bytes_after_return,
            unsigned char **data
    ) const;

    KeySym *getKeyboardMapping(
#if NeedWidePrototypes
            unsigned int	first_keycode
#else
            KeyCode first_keycode,
#endif
            int keycode_count,
            int *keysyms_per_keycode_return
    ) const;

    XModifierKeymap *getModifierMapping() const;

    int getPointerMapping(unsigned char *map_return, int nmap) const;

    Status getWindowAttributes(Window w, XWindowAttributes *window_attributes_return) const;

    int getWindowProperty(
            Window w,
            Atom property,
            long long_offset,
            long long_length,
            Bool deleteProperty,
            Atom req_type,
            Atom *actual_type_return,
            int *actual_format_return,
            unsigned long *nitems_return,
            unsigned long *bytes_after_return,
            unsigned char **prop_return
    ) const;

    [[nodiscard]] XWMHints *getWMHints(Window w) const;

    int grabControl(Bool impervious) const;

    Status grabDevice(
            int deviceid,
            Window grab_window,
            Time time,
            Cursor cursor,
            int grab_mode,
            int paired_device_mode,
            Bool owner_events,
            XIEventMask *mask
    ) const;

    int grabInterfaceButton(
            int deviceid,
            int button,
            Window grab_window,
            Cursor cursor,
            int grab_mode,
            int paired_device_mode,
            int owner_events,
            XIEventMask *mask,
            int num_modifiers,
            XIGrabModifiers *modifiers_inout
    ) const;

    int grabPointer(
            Window grab_window,
            Bool owner_events,
            unsigned int event_mask,
            int pointer_mode,
            int keyboard_mode,
            Window confine_to,
            Cursor cursor,
            Time time
    ) const;

    void hideCursor(Window win) const;

    bool isSameDisplayAs(Display* dpy2);

    KeyCode keysymToKeycode(KeySym keysym) const;

    char **listExtensions(int *nextensions_return) const;

    int nextEvent(XEvent *event_return) const;

    XIDeviceInfo *queryDevice(int deviceid, int *ndevices_return) const;

    Bool queryExtension(_Xconst char *name, int *major_opcode_return, int *first_event_return,
                        int *first_error_return) const;

    Status queryInterfaceVersion(int *major_version_inout, int *minor_version_inout) const;

    Bool queryPointer(
            Window w,
            Window *root_return,
            Window *child_return,
            int *root_x_return,
            int *root_y_return,
            int *win_x_return,
            int *win_y_return,
            unsigned int *mask_return
    ) const;

    Status queryTree(
            Window w,
            Window *root_return,
            Window *parent_return,
            Window **children_return,
            unsigned int *nchildren_return) const;

    Bool ringBell(Window win, int percent, Atom name) const;

    void sendEvent(Window window, bool propagate, long event_mask, XEvent *event_send) const;

    void selectInput(Window window, long event_mask) const;

    int selectInterfaceEvents(Window win, XIEventMask *masks, int num_masks) const;

    Status setClientPointer(Window win, int deviceid) const;

    void showCursor(Window win) const;

    Status ungrabDevice(int deviceid, Time time) const;

    Status ungrabInterfaceButton(
            int deviceid,
            int button,
            Window grab_window,
            int num_modifiers,
            XIGrabModifiers *modifiers
    ) const;

    int ungrabKey(int keycode, unsigned int modifiers, Window grab_window) const;

    int ungrabPointer(Time time) const;
};

extern std::shared_ptr<XServerProxy> global_xServer;
