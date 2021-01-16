#include "gesture.h"
#include "prefdb.h"

Gesture::Gesture(const std::vector<CursorPosition>& cp, int trigger, int button, unsigned int modifiers, bool timeout)
    : stroke(Stroke(cp)), trigger(trigger), button(button), modifiers(modifiers), timeout(timeout) {
}

std::tuple<bool, double> Gesture::compare(const Gesture& a, const Gesture& b) {
    if (!a.timeout != !b.timeout) {
        return std::make_tuple(false, 0);
    }
    if (a.button != b.button) {
        return std::make_tuple(false, 0);
    }
    if (a.trigger != b.trigger) {
        return std::make_tuple(false, 0);
    }
    if (a.modifiers != b.modifiers) {
        return std::make_tuple(false, 0);
    }

    auto cost = Stroke::compare(a.stroke, b.stroke, nullptr, nullptr);
    if (cost >= stroke_infinity) {
        return std::make_tuple(false, 0);
    }

    auto score = MAX(1.0 - 2.5 * cost, 0.0);
    if (a.timeout) {
        return std::make_tuple(score > 0.85, score) ;
    } else {
        return std::make_tuple(score > 0.7, score) ;
    }
}


std::tuple<bool, double> Gesture::compareNoButton(const Gesture& a, const Gesture& b) {
    if (!a.timeout != !b.timeout) {
        return std::make_tuple(false, 0);
    }
    if (a.trigger != b.trigger) {
        return std::make_tuple(false, 0);
    }
    if (a.modifiers != b.modifiers) {
        return std::make_tuple(false, 0);
    }

    auto cost = Stroke::compare(a.stroke, b.stroke, nullptr, nullptr);
    if (cost >= stroke_infinity) {
        return std::make_tuple(false, 0);
    }

    auto score = MAX(1.0 - 2.5 * cost, 0.0);
    if (a.timeout) {
        return std::make_tuple(score > 0.85, score) ;
    } else {
        return std::make_tuple(score > 0.7, score) ;
    }
}
