#pragma once

#include "stroke.h"
#include <gdkmm.h>
#include <vector>

#include <X11/X.h>

class Gesture {
public:
    Stroke stroke;
	int trigger;
	int button;
	unsigned int modifiers;
	bool timeout;

    Gesture(const std::vector<CursorPosition>& cp, int trigger, int button, unsigned int modifiers, bool timeout);

    [[nodiscard]] static std::tuple<bool, double> compare(const Gesture& a, const Gesture& b);

    [[nodiscard]] static std::tuple<bool, double> compareNoButton(const Gesture& a, const Gesture& b);

    [[nodiscard]] std::size_t size() const { return stroke.size(); }

	[[nodiscard]] bool trivial() const { return size() == 0 && button == 0; }
};
