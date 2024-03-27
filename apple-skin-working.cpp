// This is the original apple skin mod I made, that was added to reborn in #86

#include <cmath>

#include <GLES/gl.h>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>

#include "api.h"

static int heal_amount = 0, heal_amount_drawing = 0;
HOOK_FROM_CALL(0x27694, void, Gui_renderHearts, (Gui *gui)) {
    // Get heal_amount
    heal_amount = heal_amount_drawing = 0;

    Inventory *inventory = gui->minecraft->player->inventory;
    ItemInstance *held_ii = Inventory_getSelected(inventory);
    if (held_ii) {
        Item *held = Item_items[held_ii->id];
        if ((held->vtable->isFood(held) && held_ii->id != 325) || (held_ii->id == 325 && held_ii->auxiliary == 1)) {
            int nutrition = ((FoodItem *) held)->nutrition;
            int cur_health = gui->minecraft->player->health;
            int heal_num = std::min(cur_health + nutrition, 20) - cur_health;
            heal_amount = heal_amount_drawing = heal_num;
        }
    }

    // Call original
    Gui_renderHearts_original(gui);
}

// Background renderering
_HOOK_SINGLE_FROM_CALL(0x266f8, void, Gui_blit, (Gui *gui, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t w1, int32_t h1, int32_t w2, int32_t h2), Gui_blit_renderHearts1, static) {
    Gui_blit_renderHearts1_original(gui, x1, y1, x2, y2, w1, h1, w2, h2);
    // Render the overlay
    if (heal_amount_drawing == 1) {
        // Half heart
        Gui_blit_renderHearts1_original(gui, x1, y1, 79, 0, w1, h1, w2, h2);
        heal_amount_drawing = 0;
    } else if (heal_amount_drawing > 0) {
        // Full heart
        Gui_blit_renderHearts1_original(gui, x1, y1, 70, 0, w1, h1, w2, h2);
        heal_amount_drawing -= 2;
    }
}

_HOOK_SINGLE_FROM_CALL(0x267c8, void, Gui_blit, (Gui *gui, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t w1, int32_t h1, int32_t w2, int32_t h2), Gui_blit_renderHearts2, static) {
    // Offset the overlay
    if (x2 == 52) {
        heal_amount_drawing += 2;
    } else if (x2 == 61 && heal_amount) {
        // Half heart, flipped
        Gui_blit_renderHearts2_original(gui, x1, y1, 70, 0, w1, h1, w2, h2);
        heal_amount_drawing += 1;
    };
    Gui_blit_renderHearts2_original(gui, x1, y1, x2, y2, w1, h1, w2, h2);
    heal_amount_drawing = std::min(heal_amount_drawing, heal_amount);
}
