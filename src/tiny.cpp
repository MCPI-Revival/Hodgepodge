// Some random tiny changes
#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "init.h"
#include "api.h"

// Defusing
OVERWRITE_CALLS(PrimedTnt_interact_non_virtual, bool, PrimedTnt_interact, (PrimedTnt *self, Player *player)) {
    ItemInstance *item = Inventory_getSelected(player->inventory);
    if (item && item->id == 359) {
        item->auxiliary -= 1;
        // Spawn item
        ItemInstance i = {.count = 1, .id = 46, .auxiliary = 1};
        ItemEntity *item_entity = (ItemEntity *) EntityFactory_CreateEntity(64, self->level);
        ALLOC_CHECK(item_entity);
        ItemEntity_constructor(item_entity, self->level, self->x + 0.5f, self->y, self->z + 0.5f, &i);
        Level_addEntity(self->level, (Entity *) item_entity);
        // Remove tnt
        self->vtable->remove(self);
        return true;
    }
    return PrimedTnt_interact_non_virtual(self, player);
}

// Double doors
OVERWRITE_CALLS(DoorTile_use_non_virtual, bool, DoorTile_use, (DoorTile *self, Level *level, int x, int y, int z, Player *player)) {
    // Get data
    int data = DoorTile_getCompositeData((LevelSource *) level, x, y, z);
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
    if (level->vtable->getTile(level, x + xo, y, z + zo) == self->id) {
        data = level->vtable->getData(level, x, y, z);
        uchar new_data = level->vtable->getData(level, x + xo, y, z + zo);
        if ((data ^ 4) == (new_data ^ 4)) new_data ^= 4;
        Level_setTileAndData(level, x + xo, y, z + zo, self->id, new_data);
    }
    // Call original
    return DoorTile_use_non_virtual(self, level, x, offset ? y + 1 : y, z, player);
}

// Swing through hitboxless block
static Item_vtable *WeaponItem_vtable = (Item_vtable *) 0x10ef30;
static bool clip_through_hitboxless = true;
HOOK_FROM_CALL(0x7f5b0, HitResult, Level_clip, (Level *level, uchar *param_1, uchar *param_2, bool clip_liquids, bool param_3)) {
    return Level_clip_original_FG6_API(level, param_1, param_2, clip_liquids, clip_through_hitboxless || param_3);
}
static void on_tick(Minecraft *mc) {
    LocalPlayer *player = mc->player;
    if (player != NULL) {
        ItemInstance *item = Inventory_getSelected(player->inventory);
        clip_through_hitboxless = item != NULL && Item_items[item->id]->vtable == WeaponItem_vtable;
    }
}

// Shovel wacking
HOOK_FROM_CALL(0x852f4, bool, Mob_hurt, (Mob *self, Entity *attacker, int damage)) {
    bool ret = Mob_hurt_original_FG6_API(self, attacker, damage);
    if (!ret || !attacker || !attacker->vtable->isPlayer(attacker)) return ret;
    // Check inventory
    Player *p = (Player *) attacker;
    ItemInstance *iitem = Inventory_getSelected(p->inventory);
    if (iitem == NULL) return ret;
    // Check item
    Item *item = Item_items[iitem->id];
    if (item->vtable != (Item_vtable *) 0x10f960) return ret;
    // WACK
    float speed = ((DiggerItem *) item)->speed;
    self->vel_x += attacker->vel_x * speed;
    self->vel_y += attacker->vel_y * (speed / 7);
    self->vel_z += attacker->vel_z * speed;
    return ret;
}

Tile *Tile_init_invBedrock_injection(Tile *t) {
    Tile *ret = Tile_init(t);
    // Fix inv bedrock name
    std::string invBedrock = "invBedrock";
    t->vtable->setDescriptionId(t, &invBedrock);
    return ret;
}

void copy_with_still(std::string *into, char *from)
{
    *into = from;
    *into += "Still";
}

void copy_with_carried(std::string *into, char *from)
{
    *into = std::string(from);
    *into += "Carried";
}

static float Zombie_aiStep_getBrightness_injection(Entity *self, float param_1) {
    // Thanks goodness for Mojank's love of hacky premature optimizations
    if (self->vtable == (Entity_vtable *) 0x10cf50) return 0;
    return self->vtable->getBrightness(self, param_1);
}

static Tile_getTexture2_t CarriedTile_getTexture2 = NULL;
static int CarriedTile_getTexture2_injection(Tile *t, int face, int data) {
    if (face == 0) return 2;
    return CarriedTile_getTexture2(t, face, data);
}

static void Gui_renderProgressIndicator_GuiComponent_blit_injection(GuiComponent *self, int x_dest, int y_dest, int x_src, int y_src, int width_dest, int height_dest, int width_src, int height_src) {
    if (mc->options.third_person == 0) {
        GuiComponent_blit(self, x_dest, y_dest, x_src, y_src, width_dest, height_dest, width_src, height_src);
        return;
    }
    return;
}

static ItemInstance *Recipes_ItemInstance_injection(ItemInstance *self, Item *i, int count, UNUSED int auxiliary) {
    return ItemInstance_constructor_item_extra(self, i, count, -1);
}

char grassCarried[] = "grassCarried";
char leavesCarried[] = "leavesCarried";
static void nop(){};
__attribute__((constructor)) static void init() {
    misc_run_on_tick(on_tick);

    // No sign limit
    overwrite_call((void *) 0xd1e2c, (void *) nop);

    // Water/lava flowing lang
    overwrite_call((void *) 0xc3b54, (void *) copy_with_still);
    overwrite_call((void *) 0xc3c7c, (void *) copy_with_still);
    // Carried tile lang
    patch_address((void *) 0xc6674, (void *) grassCarried);
    patch_address((void *) 0xc6684, (void *) leavesCarried);
    // InvBedrock lang
    overwrite_call((void *) 0xc5f04, (void *) Tile_init_invBedrock_injection);

    // Fix zpigmen burning in daylight (despite being fire proof)
    overwrite_call((void *) 0x89a1c, (void *) Zombie_aiStep_getBrightness_injection);

    // Fix grass_carried's bottom texture
    CarriedTile_getTexture2 = *(Tile_getTexture2_t *) 0x1147c4;
    patch_address((void *) 0x1147c4, (void *) CarriedTile_getTexture2_injection);

    // Fix level not ticking (this is a bug with MCPI-Addons v1.2.4/v1.2.5)
    patch_address((void *) 0x109d70, (void *) ServerLevel_tick_non_virtual);

    // Fix cross hair in 3rd person
    overwrite_call((void *) 0x261b8, (void *) Gui_renderProgressIndicator_GuiComponent_blit_injection);

    // Remove charcoal (I don't like it)
    // Torch recipe #2 (removed)
    overwrite_call((void *) 0x9d8fc, (void *) nop);
    // Torch recipe #2 (changed)
    overwrite_call((void *) 0x9d970, (void *) Recipes_ItemInstance_injection);
    // Furnace recipe (changed to normal coal)
    uchar no_charcoal_patch[] = {0x00, 0x30, 0xa0, 0xe3}; // "mov r3,#0x0"
    patch((void *) 0xa09b8, no_charcoal_patch);
    // Allow charcoal to be turned into normal coal
    patch((void *) 0x95240, no_charcoal_patch);

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
