#pragma once

#include <memory>
#include "util.h"

class Modifiers : Timeout {
    static std::set<Modifiers *> all;

    static void update_mods();

    guint mods;
    Glib::ustring str;
public:
    Modifiers(guint mods_, Glib::ustring str_) : mods(mods_), str(str_) {
        all.insert(this);
        update_mods();
    }

    bool operator==(const Modifiers &m) {
        return mods == m.mods && str == m.str;
    }

    void timeout() override {
    }

    ~Modifiers() {
        all.erase(this);
        update_mods();
    }
};

bool mods_equal(std::shared_ptr<Modifiers> m1, std::shared_ptr<Modifiers> m2);
