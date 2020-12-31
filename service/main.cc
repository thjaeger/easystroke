
#include <gtkmm.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>

#include "util.h"
#include "prefdb.h"
#include "main.h"
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
Display *dpy;
Window ROOT;

std::shared_ptr<Trace> trace;

static Trace *init_trace() {
    return new Composite();
}

void Trace::start(Trace::Point p) {
    last = p;
    active = true;
    XFixesHideCursor(dpy, ROOT);
    start_();
}

void Trace::end() {
    if (!active)
        return;
    active = false;
    XFixesShowCursor(dpy, ROOT);
    end_();
}

void quit() {
    static bool dead = false;
    if (dead)
        xstate->bail_out();
    dead = true;
    Glib::RefPtr<Gio::Application> app = Gio::Application::get_default();
    xstate->queue(sigc::mem_fun(*app.operator->(), &Gio::Application::quit));
}

void sig_int(int) {
    quit();
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

    void on_quit(const Glib::VariantBase &) { quit(); }
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
    int argc;
    char **arg = command_line->get_arguments(argc);
    for (int i = 1; arg[i]; i++)
        if (!strcmp(arg[i], "send")) {
            if (!arg[++i])
                g_warning("Send requires an argument");
            else
                run_by_name(arg[i], command_line);
        } else if (!strcmp(arg[i], "quit")) {
            quit();
        } else {
            g_warning("Warning: Unknown command \"%s\".", arg[i]);
        }
    return true;
}

void App::on_activate() {
    unsetenv("DESKTOP_AUTOSTART_ID");

    signal(SIGINT, &sig_int);
    signal(SIGCHLD, SIG_IGN);

    dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        g_error("Couldn't open display.");
    }

    ROOT = DefaultRootWindow(dpy);

    xstate = new XState;
    grabber = new Grabber;
    // Force enter events to be generated
    XGrabPointer(dpy, ROOT, False, 0, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);

    trace.reset(init_trace());
    Glib::RefPtr<Gdk::Screen> screen = Gdk::Display::get_default()->get_default_screen();
    g_signal_connect(screen->gobj(), "composited-changed", &schedule_reload_trace, nullptr);
    screen->signal_size_changed().connect(sigc::ptr_fun(&schedule_reload_trace));

    XTestGrabControl(dpy, True);

    Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(dpy), Glib::IO_IN);
    io->connect(sigc::mem_fun(*xstate, &XState::handle));
    io->attach();
    hold();
}

int main(int argc, char **argv) {
    g_message("Listening...");

    App app(argc, argv, "org.easystroke.easystroke", Gio::APPLICATION_HANDLES_COMMAND_LINE);
    return app.run(argc, argv);
}

void Button::run() {
    grabber->suspend();
    xstate->fake_click(button);
    grabber->resume();
}

void SendKey::run() {
    if (!key)
        return;
    guint code = XKeysymToKeycode(dpy, key);
    XTestFakeKeyEvent(dpy, code, true, 0);
    XTestFakeKeyEvent(dpy, code, false, 0);
}

void fake_unicode(gunichar c) {
    static const KeySym numcode[10] = {XK_0, XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9};
    static const KeySym hexcode[6] = {XK_a, XK_b, XK_c, XK_d, XK_e, XK_f};

    if (log_utils::isEnabled(G_LOG_LEVEL_DEBUG)) {
        char buf[7];
        buf[g_unichar_to_utf8(c, buf)] = '\0';
        g_debug("using unicode input for character %s", buf);
    }
    XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Control_L), true, 0);
    XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L), true, 0);
    XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_u), true, 0);
    XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_u), false, 0);
    XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L), false, 0);
    XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Control_L), false, 0);
    char buf[16];
    snprintf(buf, sizeof(buf), "%x", c);
    for (int i = 0; buf[i]; i++)
        if (buf[i] >= '0' && buf[i] <= '9') {
            XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, numcode[buf[i] - '0']), true, 0);
            XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, numcode[buf[i] - '0']), false, 0);
        } else if (buf[i] >= 'a' && buf[i] <= 'f') {
            XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, hexcode[buf[i] - 'a']), true, 0);
            XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, hexcode[buf[i] - 'a']), false, 0);
        }
    XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_space), true, 0);
    XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_space), false, 0);
}

bool fake_char(gunichar c) {
    char buf[16];
    snprintf(buf, sizeof(buf), "U%04X", c);
    KeySym keysym = XStringToKeysym(buf);
    if (keysym == NoSymbol)
        return false;
    KeyCode keycode = XKeysymToKeycode(dpy, keysym);
    if (!keycode)
        return false;
    KeyCode modifier = 0;
    int n;
    KeySym *mapping = XGetKeyboardMapping(dpy, keycode, 1, &n);
    if (mapping[0] != keysym) {
        int i;
        for (i = 1; i < n; i++)
            if (mapping[i] == keysym)
                break;
        if (i == n)
            return false;
        XModifierKeymap *keymap = XGetModifierMapping(dpy);
        modifier = keymap->modifiermap[i];
        XFreeModifiermap(keymap);
    }
    XFree(mapping);
    if (modifier)
        XTestFakeKeyEvent(dpy, modifier, true, 0);
    XTestFakeKeyEvent(dpy, keycode, true, 0);
    XTestFakeKeyEvent(dpy, keycode, false, 0);
    if (modifier)
        XTestFakeKeyEvent(dpy, modifier, false, 0);
    return true;
}

void SendText::run() {
    for (Glib::ustring::iterator i = text.begin(); i != text.end(); i++)
        if (!fake_char(*i))
            fake_unicode(*i);
}

static struct {
    guint mask;
    guint sym;
} modkeys[] = {
        {GDK_SHIFT_MASK,   XK_Shift_L},
        {GDK_CONTROL_MASK, XK_Control_L},
        {GDK_MOD1_MASK,    XK_Alt_L},
        {GDK_MOD2_MASK, 0},
        {GDK_MOD3_MASK, 0},
        {GDK_MOD4_MASK, 0},
        {GDK_MOD5_MASK, 0},
        {GDK_SUPER_MASK,   XK_Super_L},
        {GDK_HYPER_MASK,   XK_Hyper_L},
        {GDK_META_MASK,    XK_Meta_L},
};
static int n_modkeys = 10;

class Modifiers : Timeout {
    static std::set<Modifiers *> all;

    static void update_mods() {
        static guint mod_state = 0;
        guint new_state = 0;
        for (auto i = all.begin(); i != all.end(); i++)
            new_state |= (*i)->mods;
        for (int i = 0; i < n_modkeys; i++) {
            guint mask = modkeys[i].mask;
            if ((mod_state & mask) ^ (new_state & mask))
                XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, modkeys[i].sym), new_state & mask, 0);
        }
        mod_state = new_state;
    }

    guint mods;
    Glib::ustring str;
public:
    Modifiers(guint mods_, Glib::ustring str_) : mods(mods_), str(str_) {
        all.insert(this);
        update_mods();
    }

    bool operator==(const Modifiers &m) {
        return mods == m.mods && str == m.str;
    }

    void timeout() override {
    }

    ~Modifiers() {
        all.erase(this);
        update_mods();
    }
};

std::set<Modifiers *> Modifiers::all;

RModifiers ModAction::prepare() {
    return RModifiers(new Modifiers(mods, get_label()));
}

RModifiers SendKey::prepare() {
    if (!mods)
        return RModifiers();
    return RModifiers(new Modifiers(mods, ModAction::get_label()));
}

bool mods_equal(RModifiers m1, RModifiers m2) {
    return m1 && m2 && *m1 == *m2;
}

void Misc::run() {
    switch (type) {
        case UNMINIMIZE:
            grabber->unminimize();
            return;
        default:
            return;
    }
}
