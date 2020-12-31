
#include <gtkmm.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>

#include "globals.h"
#include "util.h"
#include "prefdb.h"
#include "actiondb.h"
#include "trace.h"
#include "composite.h"
#include "grabber.h"
#include "handler.h"
#include "log.h"

#include <glib.h>

#include <cstring>
#include <csignal>

Source<Window> current_app_window(None);

std::shared_ptr<Trace> trace;

static Trace *init_trace() {
    return new Composite();
}

void Trace::start(Trace::Point p) {
    last = p;
    active = true;
    XFixesHideCursor(context->dpy, context->ROOT);
    start_();
}

void Trace::end() {
    if (!active)
        return;
    active = false;
    XFixesShowCursor(context->dpy, context->ROOT);
    end_();
}

class App : public Gtk::Application {
public:
    App(int &argc, char **&argv, const Glib::ustring &application_id,
        Gio::ApplicationFlags flags = Gio::APPLICATION_FLAGS_NONE) :
            Gtk::Application(argc, argv, application_id, flags) {}

protected:
    void on_activate() override;

private:
    bool local_command_line_vfunc(char **&arguments, int &exit_status) override;

    int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &) override;

    void run_by_name(const char *str, const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd_line);
};

class ReloadTrace : public Timeout {
    void timeout() {
        g_debug("Reloading gesture display");
        xstate->queue(sigc::mem_fun(*this, &ReloadTrace::reload));
    }

    void reload() { trace.reset(init_trace()); }
} reload_trace;

static void schedule_reload_trace() { reload_trace.set_timeout(1000); }

bool App::local_command_line_vfunc(char **&arg, int &exit_status) {
    if (!register_application()) {
        g_error("Failed to register the application");
    }
    activate();
    return false;
}

void App::run_by_name(const char *str, const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd_line) {
    for (auto i = actions.begin(); i != actions.end(); i++) {
        if (i->second->name == std::string(str)) {
            if (i->second->action)
                xstate->queue(sigc::bind(sigc::mem_fun(xstate, &XState::run_action), i->second->action));
            return;
        }
    }
    g_warning("No action \"%s\" defined", str);
}

int App::on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &command_line) {
    return true;
}

std::shared_ptr<AppXContext> context = nullptr;

void App::on_activate() {
    unsetenv("DESKTOP_AUTOSTART_ID");

    if (context) {
        g_error("Context already configured");
    }

    context = std::make_shared<AppXContext>();

    xstate = new XState;
    grabber = new Grabber;
    // Force enter events to be generated
    XGrabPointer(context->dpy, context->ROOT, False, 0, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    XUngrabPointer(context->dpy, CurrentTime);

    trace.reset(init_trace());
    Glib::RefPtr<Gdk::Screen> screen = Gdk::Display::get_default()->get_default_screen();
    g_signal_connect(screen->gobj(), "composited-changed", &schedule_reload_trace, nullptr);
    screen->signal_size_changed().connect(sigc::ptr_fun(&schedule_reload_trace));

    XTestGrabControl(context->dpy, True);

    Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(context->dpy), Glib::IO_IN);
    io->connect(sigc::mem_fun(*xstate, &XState::handle));
    io->attach();
    hold();
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
    if (modifier)
        XTestFakeKeyEvent(context->dpy, modifier, true, 0);
    XTestFakeKeyEvent(context->dpy, keycode, true, 0);
    XTestFakeKeyEvent(context->dpy, keycode, false, 0);
    if (modifier)
        XTestFakeKeyEvent(context->dpy, modifier, false, 0);
    return true;
}

void Actions::SendText::run() {
    for (Glib::ustring::iterator i = text.begin(); i != text.end(); i++)
        if (!fake_char(*i))
            fake_unicode(*i);
}


std::set<Modifiers *> Modifiers::all;

namespace ShutDown {
    std::function<void(int)> shutdown_handler;
    void signal_handler(int signal) { shutdown_handler(signal); }

    void onShutDown(std::function<void(int)> handler) {
        shutdown_handler = handler;
        signal(SIGINT, signal_handler);
        signal(SIGCHLD, SIG_IGN);
    }
}

int main(int argc, char *argv[]) {
    App app(argc, argv, "com.github.easy-gesture.daemon", Gio::APPLICATION_HANDLES_COMMAND_LINE);

    ShutDown::onShutDown([&](int signal) {
        g_info("Shutting down due to signal level %d", signal);
        app.quit();
    });

    return app.run();
}
