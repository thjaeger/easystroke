#include "trace.h"

#include "composite.h"
#include "xserverproxy.h"

std::shared_ptr<Trace> trace;

void resetTrace() {
    trace = std::make_shared<Composite>();
}

void Trace::start(Trace::Point p) {
    last = p;
    active = true;
    global_xServer->hideCursor(global_xServer->ROOT);
    start_();
}

void Trace::end() {
    if (!active)
        return;
    active = false;
    global_xServer->showCursor(global_xServer->ROOT);
    end_();
}
