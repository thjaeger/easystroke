
#include <gtkmm.h>
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
