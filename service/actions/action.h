#pragma once
#include <glibmm/ustring.h>

#include "modifiers.h"

namespace Actions {
    class Action {
    public:
        virtual void run() {}
        virtual std::shared_ptr<Modifiers> prepare() { return nullptr; }
        virtual const Glib::ustring get_label() const = 0;
    };

    class Command : public Action {
    public:
        std::string cmd;
        Command() = default;
        explicit Command(std::string c) : cmd(std::move(c)) {}
        virtual void run();
        virtual const Glib::ustring get_label() const { return cmd; }
    };

    class SendText : public Action {
        Glib::ustring text;
        SendText(Glib::ustring text_) : text(text_) {}
    public:
        SendText() {}

        virtual void run();
        virtual const Glib::ustring get_label() const { return text; }
    };

    class Misc : public Action {
    public:
        enum Type { NONE, UNMINIMIZE, SHOWHIDE, DISABLE };
        Type type;
    private:
        Misc(Type t) : type(t) {}
    public:
        static const char *types[5];
        Misc() {}
        virtual const Glib::ustring get_label() const;
        virtual void run();
    };


    class Click : public Action {
        virtual const Glib::ustring get_label() const { return "Click"; }
    };
}

#define IS_CLICK(act) (act && dynamic_cast<Actions::Click *>(act.get()))