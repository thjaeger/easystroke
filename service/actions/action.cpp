#include "action.h"
#include "prefdb.h"

#include "globals.h"
#include "grabber.h"
#include "handler.h"
#include "log.h"

extern std::shared_ptr<sigc::slot<void, std::shared_ptr<Gesture>>> stroke_action;

Glib::ustring ButtonInfo::get_button_text() const {
	Glib::ustring str;
	if (instant)
		str += "(Instantly) ";
	if (click_hold)
		str += "(Click & Hold) ";
	if (state == AnyModifier)
		str += Glib::ustring() + "(" + "Any Modifier" + " +) ";
	else
		str += Gtk::AccelGroup::get_label(0, (Gdk::ModifierType)state);
	return str + Glib::ustring::compose("Button %1", button);
}


void Actions::Button::run() {
    grabber->suspend();
    xstate->fake_click(button);
    grabber->resume();
}

void Actions::SendKey::run() {
    if (!key)
        return;
    guint code = XKeysymToKeycode(context->dpy, key);
    XTestFakeKeyEvent(context->dpy, code, true, 0);
    XTestFakeKeyEvent(context->dpy, code, false, 0);
}

void fake_unicode(gunichar c) {
    static const KeySym numcode[10] = {XK_0, XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9};
    static const KeySym hexcode[6] = {XK_a, XK_b, XK_c, XK_d, XK_e, XK_f};

    if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG)) {
        char buf[7];
        buf[g_unichar_to_utf8(c, buf)] = '\0';
        g_debug("using unicode input for character %s", buf);
    }
    XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, XK_Control_L), true, 0);
    XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, XK_Shift_L), true, 0);
    XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, XK_u), true, 0);
    XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, XK_u), false, 0);
    XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, XK_Shift_L), false, 0);
    XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, XK_Control_L), false, 0);
    char buf[16];
    snprintf(buf, sizeof(buf), "%x", c);
    for (int i = 0; buf[i]; i++)
        if (buf[i] >= '0' && buf[i] <= '9') {
            XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, numcode[buf[i] - '0']), true, 0);
            XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, numcode[buf[i] - '0']), false, 0);
        } else if (buf[i] >= 'a' && buf[i] <= 'f') {
            XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, hexcode[buf[i] - 'a']), true, 0);
            XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, hexcode[buf[i] - 'a']), false, 0);
        }
    XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, XK_space), true, 0);
    XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, XK_space), false, 0);
}

bool fake_char(gunichar c) {
    char buf[16];
    snprintf(buf, sizeof(buf), "U%04X", c);
    KeySym keysym = XStringToKeysym(buf);
    if (keysym == NoSymbol)
        return false;
    KeyCode keycode = XKeysymToKeycode(context->dpy, keysym);
    if (!keycode)
        return false;
    KeyCode modifier = 0;
    int n;
    KeySym *mapping = XGetKeyboardMapping(context->dpy, keycode, 1, &n);
    if (mapping[0] != keysym) {
        int i;
        for (i = 1; i < n; i++)
            if (mapping[i] == keysym)
                break;
        if (i == n)
            return false;
        XModifierKeymap *keymap = XGetModifierMapping(context->dpy);
        modifier = keymap->modifiermap[i];
        XFreeModifiermap(keymap);
    }
    XFree(mapping);
    if (modifier) {
        XTestFakeKeyEvent(context->dpy, modifier, true, 0);
    }

    XTestFakeKeyEvent(context->dpy, keycode, true, 0);
    XTestFakeKeyEvent(context->dpy, keycode, false, 0);
    if (modifier) {
        XTestFakeKeyEvent(context->dpy, modifier, false, 0);
    }
    return true;
}

void Actions::SendText::run() {
    for (Glib::ustring::iterator i = text.begin(); i != text.end(); i++)
        if (!fake_char(*i))
            fake_unicode(*i);
}

void Actions::Command::run() {
    pid_t pid = fork();
    switch (pid) {
        case 0:
            execlp("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            exit(1);
        case -1:
            g_warning("can't execute command \"%s\": fork() failed", cmd.c_str());
            break;
        default:
            break;
    }
}
