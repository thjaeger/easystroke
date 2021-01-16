#pragma once

#include <xserverproxy.h>
#include "gesture.h"

#include "events/grabber.h"
#include "actiondb.h"
#include "events/windowobserver.h"

class Handler;

class EventLoop {
private:
    std::shared_ptr<XServerProxy> xServer;

    std::unique_ptr<Handler> handler;

    static int xErrorHandler(Display *dpy2, XErrorEvent *e);
    static int xIOErrorHandler(Display *dpy2);
    int (*oldHandler)(Display *, XErrorEvent *);
    int (*oldIOHandler)(Display *);
    std::list<std::function<void()>> queued;
    std::map<int, std::string> opcodes;
public:
    EventLoop(std::shared_ptr<XServerProxy> xServer);

    std::unique_ptr<Events::WindowObserver> windowObserver;

    bool handle(Glib::IOCondition);
    void handle_event(XEvent &ev);
    void handle_xi2_event(XIDeviceEvent *event);
    void handle_raw_motion(XIRawEvent *event);
    void report_xi2_event(XIDeviceEvent *event, const char *type);

    void fake_core_button(guint b, bool press);
    void fake_click(guint b);
    void update_core_mapping();

    void remove_device(int deviceid);
    void ungrab(int deviceid);

    bool idle();

    void queue(std::function<void()> f);

    Grabber::XiDevice *current_dev;
    bool in_proximity;
    std::set<guint> xinput_pressed;
    guint modifiers;
    std::map<guint, guint> core_inv_map;
    void processQueue();

    std::shared_ptr<Grabber> grabber;
};

extern EventLoop *global_eventLoop;
