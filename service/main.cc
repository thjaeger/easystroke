
#include <gtkmm.h>
#include <X11/extensions/Xfixes.h>

#include "xserverproxy.h"
#include "util.h"
#include "actiondb.h"
#include "trace.h"
#include "grabber.h"
#include "handler.h"
#include "eventloop.h"

#include <glib.h>

#include <cstring>
#include <csignal>
#include <execinfo.h>


class App : public Gtk::Application {
private:
    std::shared_ptr<XServerProxy> xServer;

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
        global_eventLoop->queue(sigc::mem_fun(*this, &ReloadTrace::reload));
    }

    void reload() { resetTrace(); }
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

void App::on_activate() {
    unsetenv("DESKTOP_AUTOSTART_ID");

    this->xServer = XServerProxy::Open();

    global_eventLoop = new EventLoop;
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
    hold();
}

namespace ShutDown {
    std::function<void(int)> shutdown_handler;
    void signal_handler(int signal) { shutdown_handler(signal); }

    void onShutDown(std::function<void(int)> handler) {
        shutdown_handler = handler;
        signal(SIGINT, signal_handler);
        signal(SIGCHLD, SIG_IGN);
    }
}

void segFaultHandler(int sig) {
    void *array[10];
    auto size = backtrace(array, 10);

    g_warning("SegFault stack trace starting");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    g_error("Segfault :(");
}

int main(int argc, char *argv[]) {
    signal(SIGSEGV, segFaultHandler);

    App app(argc, argv, "com.github.easy-gesture.daemon", Gio::APPLICATION_HANDLES_COMMAND_LINE);

    ShutDown::onShutDown([&](int signal) {
        g_info("Shutting down due to signal level %d", signal);
        app.quit();
    });

    return app.run();
}
