#include <set>

#include "globals.h"
#include "modifiers.h"
#include "util.h"

#include <X11/extensions/XTest.h>


static struct {
    guint mask;
    guint sym;
} modkeys[] = {
        {GDK_SHIFT_MASK,   XK_Shift_L},
        {GDK_CONTROL_MASK, XK_Control_L},
        {GDK_MOD1_MASK,    XK_Alt_L},
        {GDK_MOD2_MASK,    0},
        {GDK_MOD3_MASK,    0},
        {GDK_MOD4_MASK,    0},
        {GDK_MOD5_MASK,    0},
        {GDK_SUPER_MASK,   XK_Super_L},
        {GDK_HYPER_MASK,   XK_Hyper_L},
        {GDK_META_MASK,    XK_Meta_L},
};

static int n_modkeys = 10;

void Modifiers::update_mods() {
    static guint mod_state = 0;
    guint new_state = 0;

    for (auto i : all) {
        new_state |= i->mods;
    }

    for (int i = 0; i < n_modkeys; i++) {
        guint mask = modkeys[i].mask;
        if ((mod_state & mask) ^ (new_state & mask)) {
            XTestFakeKeyEvent(context->dpy, XKeysymToKeycode(context->dpy, modkeys[i].sym), new_state & mask, 0);
        }
    }
    mod_state = new_state;
}

bool mods_equal(std::shared_ptr <Modifiers> m1, std::shared_ptr <Modifiers> m2) {
    return m1 && m2 && m1 == m2;
}
