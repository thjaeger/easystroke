#pragma once

#include "modifiers.h"

namespace Actions {
    class Action;
    class Command;
    class SendKey;
    class SendText;
    class Scroll;
    class Ignore;
    class Button;
    class Misc;
    class ModAction;

    typedef std::shared_ptr<Action> RAction;
    typedef std::shared_ptr<SendKey> RSendKey;
    typedef std::shared_ptr<SendText> RSendText;
    typedef std::shared_ptr<Scroll> RScroll;
    typedef std::shared_ptr<Ignore> RIgnore;
    typedef std::shared_ptr<Misc> RMisc;

    class Action {
    public:
        virtual void run() {}
        virtual RModifiers prepare() { return RModifiers(); }
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
        static RSendText create(Glib::ustring text) { return RSendText(new SendText(text)); }

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
        static RMisc create(Type t) { return RMisc(new Misc(t)); }
        virtual void run();
    };


    class Click : public Action {
        virtual const Glib::ustring get_label() const { return "Click"; }
    };
}

#define IS_CLICK(act) (act && dynamic_cast<Actions::Click *>(act.get()))