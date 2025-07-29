#include <cmath>

//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "dash.h"

struct Dash final : CustomItem {
    Dash(int id) : CustomItem(id) {}

    ItemInstance *use(ItemInstance *item, Level *level, Player *player) override {
        // Deplete and hurt
        if (!player->abilities.infinite_items) {
            item->count -= 1;
            player->hurt(nullptr, 4);
        }

        // Play the sounds
        float f = Mth::_random.genrand_int32();
        std::string name = "random.bow";
        level->playSound((Entity *) player, name, 0.5, 0.4 / (f * 0.4 + 0.8));

        // Dash
        constexpr float dash_speed = 3.f;

        // Convert to radians
        float pitch_rad = player->pitch * (M_PI / 180.f);
        float yaw_rad = player->yaw * (M_PI / 180.f);

        // Set velocity
        player->velocity_x += -dash_speed * sin(yaw_rad) * cos(pitch_rad);
        player->velocity_y += (-dash_speed / 3.f) * sin(pitch_rad);
        player->velocity_z += dash_speed * cos(yaw_rad) * cos(pitch_rad);

        return item;
    }
};

// Create Item
static Item *dash = NULL;
void make_dash() {
    // Construct
    dash = (new Dash(DASH_ITEM_ID - 256))->self;

    // Setup
    std::string name = "dash";
    dash->setDescriptionId(name);
    dash->max_damage = 0;
    dash->max_stack_size = 16;
    dash->texture = DASH_TEXTURE_ID;
}
