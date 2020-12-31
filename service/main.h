#pragma once

#include <string>
#include <X11/X.h>

void quit();

extern std::string config_dir;

extern "C" {
struct _XDisplay;
typedef struct _XDisplay Display;
}

extern Display *dpy;
extern Window ROOT;
