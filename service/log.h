#pragma once
#include <glib.h>

namespace log_utils {
    bool isEnabled(GLogLevelFlags level);
}
