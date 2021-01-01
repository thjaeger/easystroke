#include "trace.h"

#include "composite.h"
#include "globals.h"

std::shared_ptr<Trace> trace;

void resetTrace() {
    trace = std::make_shared<Composite>();
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
