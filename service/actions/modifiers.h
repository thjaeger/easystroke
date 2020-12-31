#pragma once

#include <memory>

class Modifiers;

typedef std::shared_ptr<Modifiers> RModifiers;

bool mods_equal(RModifiers m1, RModifiers m2);
