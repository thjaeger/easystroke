#pragma once
#include <glibmm/ustring.h>
#include "modifiers.h"

namespace Actions {
    class Action {
    public:
        virtual void run() {}
        virtual std::shared_ptr<Modifiers> prepare() { return nullptr; }
        [[nodiscard]] virtual const Glib::ustring get_label() const = 0;
    };

    class Command : public Action {
    public:
        std::string cmd;
        Command() = default;
        explicit Command(std::string c) : cmd(std::move(c)) {}
        void run() override;
        [[nodiscard]] virtual const Glib::ustring get_label() const { return cmd; }
    };

    class SendText : public Action {
        Glib::ustring text;
    public:
        SendText() = default;
        explicit SendText(Glib::ustring text_) : text(std::move(text_)) {}

        void run() override;
        [[nodiscard]] const Glib::ustring get_label() const override { return text; }
    };

    class Misc : public Action {
    public:
        enum Type { NONE, UNMINIMIZE, SHOWHIDE, DISABLE };
        Type type;
    private:
        Misc(Type t) : type(t) {}
    public:
        static const char *types[5];
        Misc() = default;
        [[nodiscard]] const Glib::ustring get_label() const override;
        void run() override;
    };

    class Click : public Action {
        [[nodiscard]] const Glib::ustring get_label() const override { return "Click"; }
    };
}
