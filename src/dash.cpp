#include <math.h>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "dash.h"

static ItemInstance *Dash_use(UNUSED Item *self, ItemInstance *item, Level *level, Player *player) {
    // Deplete and hurt
    if (!player->infinite_items) {
        item->count -= 1;
        player->vtable->hurt(player, NULL, 4);
    }

    // Play the sounds
    float f = Random_genrand_int32(&Mth__random);
    std::string name = "random.bow";
    Level_playSound(level, (Entity *) player, &name, 0.5, 0.4 / (f * 0.4 + 0.8));

    // Dash
    constexpr float dash_speed = 3.f;

    // Convert to radians
    float pitch_rad = player->pitch * (M_PI / 180.f);
    float yaw_rad = player->yaw * (M_PI / 180.f);

    // Set velocity
    player->vel_x += -dash_speed * sin(yaw_rad) * cos(pitch_rad);
    player->vel_y += (-dash_speed / 3.f) * sin(pitch_rad);
    player->vel_z += dash_speed * cos(yaw_rad) * cos(pitch_rad);

    return item;
}

static Item_vtable *get_dash_item_vtable() {
    static Item_vtable *vtable = NULL;
    if (vtable == NULL) {
        // Init
        vtable = dup_Item_vtable(Item_vtable_base);
        ALLOC_CHECK(vtable);

        // Modify
        vtable->use = Dash_use;
    }
    return vtable;
}

// Create Item
static Item *dash = NULL;
void make_dash() {
    // Construct
    dash = new Item();
    ALLOC_CHECK(dash);
    Item_constructor(dash, DASH_ITEM_ID - 256);

    // Set VTable
    dash->vtable = get_dash_item_vtable();

    // Setup
    std::string name = "dash";
    dash->vtable->setDescriptionId(dash, &name);
    dash->max_damage = 0;
    dash->max_stack_size = 16;
    dash->texture = DASH_TEXTURE_ID;
}
