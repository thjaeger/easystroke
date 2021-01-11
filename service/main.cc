
#include <gtkmm.h>
#include "xserverproxy.h"
#include "util.h"
#include "trace.h"
#include "events/grabber.h"
#include "events/handler.h"
#include "events/eventloop.h"

#include <glib.h>

#include <csignal>
#include <utility>
#include <execinfo.h>


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

    auto xServer = XServerProxy::Open();
    global_eventLoop = new EventLoop(xServer);

    hold();
}

namespace ShutDown {
    std::function<void(int)> shutdown_handler;
    void signal_handler(int signal) { shutdown_handler(signal); }

    void onShutDown(std::function<void(int)> handler) {
        shutdown_handler = std::move(handler);
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
