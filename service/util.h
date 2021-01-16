#pragma once

#include <glibmm.h>

class Timeout {
    sigc::connection connection;

    // We have to account for the possibility that timeout() destroys the object
    bool to() {
        timeout();
        return false;
    }

public:
    Timeout() = default;

protected:
    virtual void timeout() = 0;

public:
    void remove_timeout() {
        connection.disconnect();
    }

    void set_timeout(int ms) {
        remove_timeout();
        connection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &Timeout::to), ms);
    }

    virtual ~Timeout() {
        remove_timeout();
    }
};
