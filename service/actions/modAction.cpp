#include "modAction.h"

#include <gtkmm.h>

namespace Actions {
    std::shared_ptr<Modifiers> ModAction::prepare() {
        return std::make_shared<Modifiers>(mods, get_label());
    }

    const Glib::ustring ModAction::get_label() const {
        if (!mods)
            return "No Modifiers";
        Glib::ustring label = Gtk::AccelGroup::get_label(0, mods);
        return label.substr(0, label.size() - 1);
    }

    std::shared_ptr<Modifiers> SendKey::prepare() {
        if (!mods) {
            return nullptr;
        }
        return std::make_shared<Modifiers>(mods, ModAction::get_label());
    }

    const Glib::ustring SendKey::get_label() const {
        return Gtk::AccelGroup::get_label(key, mods);
    }

    const Glib::ustring Scroll::get_label() const {
        if (mods)
            return ModAction::get_label() + " + Scroll";
        else
            return "Scroll";
    }

    const Glib::ustring Ignore::get_label() const {
        if (mods)
            return ModAction::get_label();
        else
            return "Ignore";
    }
}
