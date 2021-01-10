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
    context->xServer->hideCursor(context->xServer->ROOT);
    start_();
}

void Trace::end() {
    if (!active)
        return;
    active = false;
    context->xServer->showCursor(context->xServer->ROOT);
    end_();
}
