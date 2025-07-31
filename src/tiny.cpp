// Some random tiny changes
//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "init.h"
#include "api.h"

// Defusing
static bool PrimedTnt_interact_injection(PrimedTnt *self, Player *player) {
    ItemInstance *item = player->inventory->getSelected();
    if (item && item->id == 359) {
        item->auxiliary -= 1;
        // Spawn item
        ItemInstance i = {.count = 1, .id = 46, .auxiliary = 1};
        ItemEntity *item_entity = (ItemEntity *) EntityFactory::CreateEntity(64, self->level);
        item_entity->constructor(self->level, self->x + 0.5f, self->y, self->z + 0.5f, i);
        self->level->addEntity((Entity *) item_entity);
        // Remove tnt
        self->remove();
        return true;
    }
    return Entity_interact->get(false)((Entity *) self, player);
}

// Double doors
OVERWRITE_CALLS(DoorTile_use, bool, DoorTile_use_injection, (DoorTile_use_t original, DoorTile *self, Level *level, int x, int y, int z, Player *player)) {
    // Get data
    int data = DoorTile::getCompositeData((LevelSource *) level, x, y, z);
    bool offset = false;
    if (data & 8) {
        // It's upper
        y -= 1;
        offset = true;
    }
    // Get angle
    int dir = data & 3;
    int xo = 0, zo = 0;
    if (dir == 0) {
        zo = 1;
    } else if (dir == 2) {
        zo = -1;
    } else if (dir == 1) {
        xo = -1;
    } else if (dir == 3) {
        xo = 1;
    }
    // Flip
    if (data & 16) {
        xo = -xo;
        zo = -zo;
    }
    // Set
    if (level->getTile(x + xo, y, z + zo) == self->id) {
        data = level->getData(x, y, z);
        uchar new_data = level->getData(x + xo, y, z + zo);
        if ((data ^ 4) == (new_data ^ 4)) new_data ^= 4;
        level->setTileAndData(x + xo, y, z + zo, self->id, new_data);
    }
    // Call original
    return original(self, level, x, offset ? y + 1 : y, z, player);
}

// Swing through hitboxless block
static bool clip_through_hitboxless = true;
OVERWRITE_CALLS(
    Level_clip, HitResult, Level_clip_injection, (Level_clip_t original, Level *level, const Vec3 &param_1, const Vec3 &param_2, bool clip_liquids, bool param_3)
) {
    return original(level, param_1, param_2, clip_liquids, clip_through_hitboxless || param_3);
}
static void on_tick(Minecraft *mc) {
    LocalPlayer *player = mc->player;
    if (player != NULL) {
        ItemInstance *item = player->inventory->getSelected();
        clip_through_hitboxless = item != NULL && Item::items[item->id]->vtable == WeaponItem_vtable;
    }
}

// Shovel wacking
OVERWRITE_CALLS(Mob_hurt, bool, Mob_hurt_injection, (Mob_hurt_t original, Mob *self, Entity *attacker, int damage)) {
    bool ret = original(self, attacker, damage);
    if (!ret || !attacker || !attacker->isPlayer()) return ret;
    // Check inventory
    Player *p = (Player *) attacker;
    ItemInstance *iitem = p->inventory->getSelected();
    if (iitem == NULL) return ret;
    // Check item
    Item *item = Item::items[iitem->id];
    if (item->vtable != ShovelItem_vtable) return ret;
    // WACK
    float speed = ((DiggerItem *) item)->speed;
    self->velocity_x += attacker->velocity_x * speed;
    self->velocity_y += attacker->velocity_y * (speed / 7);
    self->velocity_z += attacker->velocity_z * speed;
    return ret;
}

// Save tile entities on leaving the world, not just pausing
OVERWRITE_CALLS(
    Minecraft_leaveGame,
    void, Minecraft_leaveGame_injection, (Minecraft_leaveGame_t orignal, Minecraft *mc, bool save_remote)
) {
    if (mc->generating_level || !mc->level_generation_signal) return;
    if (mc->level && (!mc->level->is_client_side || save_remote)) {
        mc->level->saveLevelData();
        mc->level->saveGame();
    }
    orignal(mc, save_remote);
}

static void nop(){};
__attribute__((constructor)) static void init() {
    misc_run_on_tick(on_tick);

    // Defuse TNT
    patch_vtable(PrimedTnt_interact, PrimedTnt_interact_injection);

    // No sign limit
    overwrite_call_manual((void *) 0xd1e2c, (void *) nop);

#if 0
    // Infinite worlds, too buggy for now, but I'm going to do it for real one day
    // Also, don't load it in a world you care about ;)
    overwrite_call((void *)  0xa28ec, (void *) nop);
    uchar nop_patch[4] = {0x00, 0xf0, 0x20, 0xe3};
    patch((void *) 0xa28e4, nop_patch);
    overwrite_call((void *) 0xb05d8, (void *) nop);
    // Fix Level::clip
    //patch((void *) 0xa41c4, nop_patch);
    //patch((void *) 0xa41d0, nop_patch);
    // Fix face culling
    // Void culling
    //uchar void_culling_patch[4] = {0x00, 0x80, 0xa0, 0xe3}; // "mov r8,#0x0" (movcc -> mov)
    //patch((void *) 0xc2d98, void_culling_patch);
    // Dist culling
    patch((void *) 0xc2dd0, nop_patch);
    patch((void *) 0xc2df4, nop_patch);
    patch((void *) 0xc2e14, nop_patch);
    patch((void *) 0xc2e2c, nop_patch);
#endif
}
