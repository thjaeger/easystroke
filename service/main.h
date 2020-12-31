#pragma once

#include <string>
#include <X11/X.h>

bool is_file(std::string filename);
bool is_dir(std::string dirname);
void quit();

extern std::string config_dir;
extern const char *prefs_versions[];
extern const char *actions_versions[];
extern bool experimental;

extern "C" {
struct _XDisplay;
typedef struct _XDisplay Display;
}

extern Display *dpy;
extern Window ROOT;
