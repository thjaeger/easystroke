#include "log.h"
#include <cstdlib>

namespace log_utils {
    bool isEnabled(GLogLevelFlags level) {
        return std::getenv("G_MESSAGES_DEBUG") == "all";
    }
}
