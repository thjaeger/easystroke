/*
 * Copyright (c) 2008-2009, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "win.h"
#include "main.h"
#include "actiondb.h"
#include "prefdb.h"
#include "trace.h"
#include "composite.h"
#include "grabber.h"
#include "handler.h"

#include <glibmm/i18n.h>

#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>

#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>

extern Source<bool> disabled;

bool experimental = false;
int verbosity = 100;
const char *prefs_versions[] = {"-0.5.5", "-0.4.1", "-0.4.0", "", nullptr};
const char *actions_versions[] = {"-0.5.6", "-0.4.1", "-0.4.0", "", nullptr};
Source<Window> current_app_window(None);
std::string config_dir;
Display *dpy;
Window ROOT;

boost::shared_ptr<Trace> trace;

static ActionDBWatcher *action_watcher = 0;

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

class App : public Gtk::Application, Base {
public:
    App(int &argc, char **&argv, const Glib::ustring &application_id,
        Gio::ApplicationFlags flags = Gio::APPLICATION_FLAGS_NONE) :
            Gtk::Application(argc, argv, application_id, flags), remote(false), enabled(nullptr) {}

    ~App();

    static void usage(const char *me);

    static void version();

protected:
    virtual void on_startup();

    virtual void on_activate();

private:
    virtual bool local_command_line_vfunc(char **&arguments, int &exit_status);

    int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &);

    void run_by_name(const char *str, const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd_line);

    void create_config_dir();

    void on_quit(const Glib::VariantBase &) { quit(); }

    virtual void notify() {
        g_simple_action_set_state(enabled, g_variant_new("b", !disabled.get()));
    }

    bool remote;
    GSimpleAction *enabled;
};

class ReloadTrace : public Timeout {
    void timeout() {
        if (verbosity >= 2)
            printf("Reloading gesture display\n");
        xstate->queue(sigc::mem_fun(*this, &ReloadTrace::reload));
    }

    void reload() { trace.reset(init_trace()); }
} reload_trace;

static void schedule_reload_trace() { reload_trace.set_timeout(1000); }

extern const char *gui_buffer;

bool App::local_command_line_vfunc(char **&arg, int &exit_status) {
    int i = 1;
    while (arg[i] && arg[i][0] == '-') {
        if (arg[i][1] == '-') {
            if (!strcmp(arg[i], "--experimental")) {
                experimental = true;
            } else if (!strcmp(arg[i], "--verbose")) {
                verbosity++;
            } else if (!strcmp(arg[i], "--help")) {
                usage(arg[0]);
                exit_status = EXIT_SUCCESS;
                return true;
            } else if (!strcmp(arg[i], "--version")) {
                version();
                exit_status = EXIT_SUCCESS;
                return true;
            } else if (!strcmp(arg[i], "--config-dir")) {
                if (!arg[++i]) {
                    printf("Error: Option --config-dir requires an argument.\n");
                    exit_status = EXIT_FAILURE;
                    return true;
                }
                config_dir = arg[i];
            } else {
                printf("Error: Unknown option %s\n", arg[i]);
                exit_status = EXIT_FAILURE;
                return true;
            }
        } else {
            for (int j = 1; arg[i][j]; j++)
                switch (arg[i][j]) {
                    case 'c':
                        if (arg[i][j + 1] || !arg[i + 1]) {
                            printf("Error: Option -c requires an argument.\n");
                            exit_status = EXIT_FAILURE;
                            return true;
                        }
                        config_dir = arg[++i];
                        break;
                    case 'e':
                        experimental = true;
                        break;
                    case 'v':
                        verbosity++;
                        break;
                    case 'h':
                        usage(arg[0]);
                        exit_status = EXIT_SUCCESS;
                        return true;
                    default:
                        printf("Error: Unknown option -%c\n", arg[i][j]);
                        exit_status = EXIT_FAILURE;
                        return true;
                }
        }
        i++;
    }

    if (i > 1) {
        for (int j = 1; j < i; j++) {
            g_free(arg[j]);
            arg[j] = 0;
        }
        for (int j = 0; arg[j + i]; j++) {
            arg[j + 1] = arg[j + i];
            arg[j + i] = 0;
        }
    }

    if (!register_application()) {
        printf("Failed to register the application\n");
        exit_status = EXIT_FAILURE;
        return true;
    }
    activate();
    return false;
}

void App::run_by_name(const char *str, const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd_line) {
    for (ActionDB::const_iterator i = actions.begin(); i != actions.end(); i++) {
        if (i->second.name == std::string(str)) {
            if (i->second.action)
                xstate->queue(sigc::bind(sigc::mem_fun(xstate, &XState::run_action), i->second.action));
            return;
        }
    }
    char *msg;
    asprintf(&msg, _("Warning: No action \"%s\" defined\n"), str);
    cmd_line->print(msg);
    free(msg);
}

int App::on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &command_line) {
    int argc;
    char **arg = command_line->get_arguments(argc);
    for (int i = 1; arg[i]; i++)
        if (!strcmp(arg[i], "send")) {
            if (!arg[++i])
                printf("Warning: Send requires an argument\n");
            else
                run_by_name(arg[i], command_line);
        } else if (!strcmp(arg[i], "disable")) {
            disabled.set(true);
        } else if (!strcmp(arg[i], "enable")) {
            disabled.set(false);
        }  else if (!strcmp(arg[i], "quit")) {
            quit();
        } else {
            char *msg;
            asprintf(&msg, "Warning: Unknown command \"%s\".\n", arg[i]);
            command_line->print(msg);
            free(msg);
        }
    remote = true;
    return true;
}

static void enabled_activated(GSimpleAction *simple, GVariant *parameter, gpointer user_data) {
    disabled.set(!disabled.get());
}

void App::on_startup() {
    Gtk::Application::on_startup();

    Glib::RefPtr<Gio::SimpleAction> action;

    action = Gio::SimpleAction::create("quit");
    action->signal_activate().connect(sigc::mem_fun(*this, &App::on_quit));
    add_action(action);

    enabled = g_simple_action_new_stateful("enabled", 0, g_variant_new_boolean(false));
    g_signal_connect(G_OBJECT(enabled), "activate", G_CALLBACK(enabled_activated), this);
    g_action_map_add_action(G_ACTION_MAP(gobj()), G_ACTION(enabled));

    Glib::RefPtr<Gio::Menu> menu = Gio::Menu::create();
    menu->append(_("Enabled"), "app.enabled");
    menu->append(_("About"), "app.about");
    menu->append(_("Quit"), "app.quit");
    set_app_menu(menu);

    disabled.connect(this);
    notify();
}

void App::on_activate() {
    bindtextdomain("easystroke", is_dir("po") ? "po" : LOCALEDIR);
    bind_textdomain_codeset("easystroke", "UTF-8");
    textdomain("easystroke");

    create_config_dir();
    unsetenv("DESKTOP_AUTOSTART_ID");

    signal(SIGINT, &sig_int);
    signal(SIGCHLD, SIG_IGN);

    dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        printf(_("Couldn't open display.\n"));
        exit(EXIT_FAILURE);
    }

    ROOT = DefaultRootWindow(dpy);

    prefs.init();
    action_watcher = new ActionDBWatcher;
    action_watcher->init();

    xstate = new XState;
    grabber = new Grabber;
    // Force enter events to be generated
    XGrabPointer(dpy, ROOT, False, 0, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);

    trace.reset(init_trace());
    Glib::RefPtr<Gdk::Screen> screen = Gdk::Display::get_default()->get_default_screen();
    g_signal_connect(screen->gobj(), "composited-changed", &schedule_reload_trace, nullptr);
    screen->signal_size_changed().connect(sigc::ptr_fun(&schedule_reload_trace));
    Notifier *trace_notify = new Notifier(sigc::ptr_fun(&schedule_reload_trace));
    prefs.color.connect(trace_notify);

    XTestGrabControl(dpy, True);

    Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(dpy), Glib::IO_IN);
    io->connect(sigc::mem_fun(*xstate, &XState::handle));
    io->attach();
    try {
        widgets = Gtk::Builder::create_from_string(gui_buffer);
    } catch (Gtk::BuilderError &e) {
        printf("Error building GUI: %s\n", e.what().c_str());
        exit(EXIT_FAILURE);
    }
    hold();
}

void App::usage(const char *me) {
    printf("The full easystroke documentation is available at the following address:\n");
    printf("\n");
    printf("http://easystroke.wiki.sourceforge.net/Documentation#content\n");
    printf("\n");
    printf("Usage: %s [OPTION]... [COMMAND]...\n", me);
    printf("\n");
    printf("Commands:\n");
    printf("  send <action_name>     Execute action <action_name>\n");
    printf("  show                   Show configuration window\n");
    printf("  hide                   Hide configuration window\n");
    printf("  disable                Disable easystroke\n");
    printf("  enable                 Enable easystroke\n");
    printf("  about                  Show about dialog\n");
    printf("  quit                   Quit easystroke\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config-dir <dir> Directory for config files\n");
    printf("  -e  --experimental     Start in experimental mode\n");
    printf("  -v, --verbose          Increase verbosity level\n");
    printf("  -h, --help             Display this help and exit\n");
    printf("      --version          Output version information and exit\n");
}

extern const char *version_string;

void App::version() {
    printf("easystroke %s\n", version_string);
    printf("\n");
    printf("Written by Thomas Jaeger <ThJaeger@gmail.com>.\n");
    exit(EXIT_SUCCESS);
}

void App::create_config_dir() {
    if (config_dir == "") {
        config_dir = getenv("HOME");
        config_dir += "/.easystroke";
    }
    struct stat st;
    char *name = realpath(config_dir.c_str(), nullptr);

    // check if the directory does not exist
    if (lstat(name, &st) == -1) {
        if (mkdir(config_dir.c_str(), 0777) == -1) {
            printf(_("Error: Couldn't create configuration directory \"%s\"\n"), config_dir.c_str());
            exit(EXIT_FAILURE);
        }
    } else {
        if (!S_ISDIR(st.st_mode)) {
            printf(_("Error: \"%s\" is not a directory\n"), config_dir.c_str());
            exit(EXIT_FAILURE);
        }


    }
    free(name);
    config_dir += "/";
}

App::~App() {
}

int main(int argc, char **argv) {
    printf("Listening...");

    if (0) {
        RStroke trefoil = Stroke::trefoil();
        trefoil->draw_svg("easystroke.svg");
        exit(EXIT_SUCCESS);
    }
    // GtkApplication needs dbus to even invoke the local_command_line function
    if (argc > 1 && !strcmp(argv[1], "--help")) {
        App::usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (argc > 1 && !strcmp(argv[1], "--version")) {
        App::usage(argv[0]);
        return EXIT_SUCCESS;
    }

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

    if (verbosity >= 3) {
        char buf[7];
        buf[g_unichar_to_utf8(c, buf)] = '\0';
        printf("using unicode input for character %s\n", buf);
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
        for (std::set<Modifiers *>::iterator i = all.begin(); i != all.end(); i++)
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

    virtual void timeout() {
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
        case DISABLE:
            disabled.set(!disabled.get());
            return;
        default:
            return;
    }
}

bool is_file(std::string filename) {
    struct stat st;
    return lstat(filename.c_str(), &st) != -1 && S_ISREG(st.st_mode);
}

bool is_dir(std::string dirname) {
    struct stat st;
    return lstat(dirname.c_str(), &st) != -1 && S_ISDIR(st.st_mode);
}
