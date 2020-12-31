#include "modAction.h"

std::shared_ptr<Modifiers> Actions::ModAction::prepare() {
    return std::make_shared<Modifiers>(mods, get_label());
}

std::shared_ptr<Modifiers> Actions::SendKey::prepare() {
    if (!mods) {
        return nullptr;
    }
    return std::make_shared<Modifiers>(mods, ModAction::get_label());
}
