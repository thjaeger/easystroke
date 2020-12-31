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

        virtual RModifiers prepare();

    public:
        virtual const Glib::ustring get_label() const;
    };

    class SendKey : public ModAction {
        guint key;

        SendKey(guint key_, Gdk::ModifierType mods) :
                ModAction(mods), key(key_) {}

    public:
        SendKey() {}

        static RSendKey create(guint key, Gdk::ModifierType mods) {
            return RSendKey(new SendKey(key, mods));
        }

        virtual void run();

        virtual RModifiers prepare();

        virtual const Glib::ustring get_label() const;
    };


    class Scroll : public ModAction {
        Scroll(Gdk::ModifierType mods) : ModAction(mods) {}

    public:
        Scroll() {}

        static RScroll create(Gdk::ModifierType mods) { return RScroll(new Scroll(mods)); }

        virtual const Glib::ustring get_label() const;
    };


    class Ignore : public ModAction {
        Ignore(Gdk::ModifierType mods) : ModAction(mods) {}

    public:
        Ignore() {}

        static RIgnore create(Gdk::ModifierType mods) { return RIgnore(new Ignore(mods)); }

        virtual const Glib::ustring get_label() const;
    };


    class Button : public ModAction {
        guint button;
    public:
        Button() {}
        Button(Gdk::ModifierType mods, guint button_) : ModAction(mods), button(button_) {}

        ButtonInfo get_button_info() const;

        static unsigned int get_button(RAction act) {
            if (!act)
                return 0;
            Button *b = dynamic_cast<Button *>(act.get());
            if (!b)
                return 0;
            return b->get_button_info().button;
        }

        virtual const Glib::ustring get_label() const;

        virtual void run();
    };
}

#define IS_KEY(act) (act && dynamic_cast<Actions::SendKey *>(act.get()))
#define IS_SCROLL(act) (act && dynamic_cast<Actions::Scroll *>(act.get()))
#define IS_IGNORE(act) (act && dynamic_cast<Actions::Ignore *>(act.get()))
#define IF_BUTTON(act, b) if (unsigned int b = Actions::Button::get_button(act))
