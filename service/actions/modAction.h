#pragma once
#include <string>
#include <map>
#include <set>
#include <iostream>
#include <utility>

#include "action.h"
#include "gesture.h"
#include "prefdb.h"

namespace Actions {
    class ModAction : public Action {
    protected:
        ModAction() {}

        Gdk::ModifierType mods;

        ModAction(Gdk::ModifierType mods_) : mods(mods_) {}

        virtual std::shared_ptr<Modifiers> prepare();

    public:
        virtual const Glib::ustring get_label() const;
    };

    class SendKey : public ModAction {
        guint key;

        SendKey(guint key_, Gdk::ModifierType mods) :
                ModAction(mods), key(key_) {}

    public:
        SendKey() {}

        virtual void run();

        virtual std::shared_ptr<Modifiers> prepare();

        virtual const Glib::ustring get_label() const;
    };


    class Scroll : public ModAction {
        Scroll(Gdk::ModifierType mods) : ModAction(mods) {}

    public:
        Scroll() {}

        virtual const Glib::ustring get_label() const;
    };


    class Ignore : public ModAction {
        Ignore(Gdk::ModifierType mods) : ModAction(mods) {}

    public:
        Ignore() {}

        virtual const Glib::ustring get_label() const;
    };


    class Button : public ModAction {
        guint button;
    public:
        Button() {}
        Button(Gdk::ModifierType mods, guint button_) : ModAction(mods), button(button_) {}

        ButtonInfo get_button_info() const;

        static unsigned int get_button(std::shared_ptr<Action> act) {
            if (!act) {
                return 0;
            }

            if (auto b = std::dynamic_pointer_cast<Button>(act)) {
                return b->get_button_info().button;
            }

            return 0;
        }

        virtual const Glib::ustring get_label() const;

        virtual void run();
    };
}
