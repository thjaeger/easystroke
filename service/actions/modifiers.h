#pragma once

#include <memory>
#include <set>
#include <utility>
#include "util.h"

class Modifiers : Timeout {
    static std::set<Modifiers *> all;

    static void update_mods();

    guint mods;
    Glib::ustring str;
public:
    Modifiers(guint mods_, Glib::ustring str_) : mods(mods_), str(std::move(str_)) {
        all.insert(this);
        update_mods();
    }

    bool operator==(const Modifiers &m) {
        return mods == m.mods && str == m.str;
    }

    void timeout() override {
    }

    ~Modifiers() override {
        all.erase(this);
        update_mods();
    }
};

bool mods_equal(std::shared_ptr<Modifiers> m1, std::shared_ptr<Modifiers> m2);
