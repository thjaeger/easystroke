#pragma once

#include <utility>

#include "xserverproxy.h"

class AppXContext {
public:
    AppXContext(std::shared_ptr<XServerProxy> xServer): xServer(std::move(xServer)) {
    }

    std::shared_ptr<XServerProxy> xServer;
};
