#include <cmath>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "nether_wand.h"
#include "pedestal.h"
#include "ender_pearls.h"
#include "redstone.h"
#include "dash.h"

int transform_id(int *id, int data, bool *should_heal) {
    switch (*id) {
        // Cobble -> netherack
        case 4:
            *id = 87;
            return 0;
        // Sand -> gravel
        case 12:
            *id = 13;
            return 0;
        // Bricks -> nether bricks
        case 45:
            *id = 112;
            return 0;
        // Flowers/saplings -> dead bush
        case 6:
        case 31:
        case 37:
        case 38:
            *id = 31;
            return 0;
        // Brown Mushroom -> Red Mushroom
        case 39:
            *id = 40;
            return 0;
        // Red Mushroom -> Brown Mushroom
        case 40:
            *id = 39;
            return 0;

        /// ITEMS
        // Seeds -> Melon Seeds
        case 295:
            *id = 362;
            return 0;
        // Sugar/Gunpowder -> glowstone dust
        case 353:
        case 289:
            *id = 348;
            return 0;
        // Cactus -> slime ball
        case 81:
            *id = 341;
            return 0;
        // Non-lava bucket -> empty bucket
        case 325:
            if (data != 10) return 0;
            if (should_heal) *should_heal = true;
            return 0;
        // Diamonds -> ender pearls
        case 264:
            *id = ENDER_PEARL_ITEM_ID;
            return 0;
        // Redstone -> thrust crystal
        case REDSTONE_ID:
            *id = DASH_ITEM_ID;
            return 0;

        // Anything else
        default:
            return -1;
    }
}

static void show_particles_and_pop(Level *level, Entity *src, int x, int y, int z, bool small) {
    // Play the sounds
    float f = Random_genrand_int32(&Mth__random);
    std::string name = "random.pop";
    Level_playSound(level, src, &name, 0.5, 0.4 / (f * 0.4 + 0.8));

    // Show the particles
    std::string part = "flame";
    float speed = small ? 0.15 : 0.3;
    y += small ? 0 : 0.5;
    for (float ym : {speed, -speed}) {
        // Sides
        for (int i = 0; i < 16; i++) {
            Level_addParticle(level, &part, x + 0.5, y, z + 0.5, (sin(i * (360/8))) * speed, (i > 8) * ym, (cos(i * (360/8))) * speed, 0);
        }
        // Top/bottom
        Level_addParticle(level, &part, x + 0.5, y, z + 0.5, 0, ym, 0, 0);
    }
}

bool transmutate_pedestal(Level *level, Player *player, int x, int y, int z, ItemInstance *item) {
    // Get
    if (level->vtable->getTile(level, x, y, z) != PEDESTAL_ID) return false;
    PedestalTileEntity *pedestal_te = (PedestalTileEntity *) Level_getTileEntity(level, x, y, z);
    if (!pedestal_te) return false;
    // Transmutate
    bool should_heal = false;
    int id = pedestal_te->item.id;
    int aux = transform_id(&id, pedestal_te->item.auxiliary, &should_heal);
    // Set
    if (aux != -1 && (id != pedestal_te->item.id || pedestal_te->item.auxiliary != aux)) {
        pedestal_te->item.id = id;
        pedestal_te->item.auxiliary = aux;
        show_particles_and_pop(level, (Entity *) player, x, (float) y + 1.3f, z, true);
        // Heal
        if (should_heal && item) item->auxiliary = 0;
        return true;
    }
    return false;
}

static int NetherWandItem_useOn(UNUSED Item *self, ItemInstance *item, Player *player, Level *level, int x, int y, int z, UNUSED int hit_side, UNUSED float hit_x, UNUSED float hit_y, UNUSED float hit_z) {
    // Deplete
    if (!player->infinite_items && item) {
        item->auxiliary -= 1;
    }

    // Let the transmutation begin
    int id = level->vtable->getTile(level, x, y, z);
    if (id == PEDESTAL_ID) {
        // Transmutate the pedestal
        return transmutate_pedestal(level, player, x, y, z, item);
    }
    // Transmutate the block
    int old_id = id, old_data = level->vtable->getData(level, x, y, z);
    int data = transform_id(&id, old_data, NULL);
    if (data == -1 || (old_id == id && old_data == data)) return 1;
    if (id < 256) {
        // Block
        Level_setTileAndData(level, x, y, z, id, data);
    } else {
        // Item
        ItemInstance i = {.count = 1, .id = id, .auxiliary = data};
        ItemEntity *item_entity = (ItemEntity *) EntityFactory_CreateEntity(64, level);
        ALLOC_CHECK(item_entity);
        ItemEntity_constructor(item_entity, level, x + 0.5f, y, z + 0.5f, &i);
        Entity_moveTo((Entity *) item_entity, x + 0.5f, y, z + 0.5f, 0, 0);
        Level_addEntity(level, (Entity *) item_entity);
        Level_setTileAndData(level, x, y, z, 0, 0);
    }
    show_particles_and_pop(level, (Entity *) player, x, y, z, false);

    return 1;
}

static void NetherWandItem_interactEnemy(UNUSED Item *self, UNUSED ItemInstance *item, Mob *mob) {
    if (mob->vtable->getEntityTypeId(mob) == 12) {
        // Zombify!
        Mob *zombpig = MobFactory_CreateMob(36, mob->level);
        ALLOC_CHECK(zombpig);
        Entity_moveTo((Entity *) zombpig, mob->x, mob->y, mob->z, mob->yaw, mob->pitch);
        Level_addEntity(mob->level, (Entity *) zombpig);
        mob->vtable->remove(mob);
        // Pop
        show_particles_and_pop(zombpig->level, (Entity *) zombpig, zombpig->x, zombpig->y, zombpig->z, false);
    }
}

OVERWRITE_CALLS(GameMode_attack_non_virtual, void, GameMode_attack_injection, (GameMode *self, Player *player, Entity *target)) {
    ItemInstance *held = Inventory_getSelected(player->inventory);
    if (held && held->id == NETHER_WAND_ID) {
        held->auxiliary -= 1;
        target->fire_timer = std::max(target->fire_timer, 100);
    }
    return GameMode_attack_non_virtual(self, player, target);
}

static Item_vtable *get_nether_wand_item_vtable() {
    static Item_vtable *vtable = NULL;
    if (vtable == NULL) {
        // Init
        vtable = dup_Item_vtable(Item_vtable_base);
        ALLOC_CHECK(vtable);

        // Modify
        vtable->useOn = NetherWandItem_useOn;
        vtable->interactEnemy = NetherWandItem_interactEnemy;
        //vtable->hurtEnemy = NetherWandItem_hurtEnemy;
    }
    return vtable;
}

// Create Item
static Item *nether_wand = NULL;
void make_nether_wand() {
    // Construct
    nether_wand = alloc_Item();
    ALLOC_CHECK(nether_wand);
    Item_constructor(nether_wand, NETHER_WAND_ID - 256);

    // Set VTable
    nether_wand->vtable = get_nether_wand_item_vtable();

    // Setup
    std::string name = "nether_wand";
    nether_wand->vtable->setDescriptionId(nether_wand, &name);
    nether_wand->max_damage = 60;
    nether_wand->is_stacked_by_data = true;
    nether_wand->max_stack_size = 1;
    nether_wand->equipped = 1;
    nether_wand->category = 2;
    nether_wand->texture = NETHER_WAND_TEXTURE;
}
