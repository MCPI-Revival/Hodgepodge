#include <cmath>

//#include <libreborn/libreborn.h>
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
        // Glass -> quartz
        case 20:
            *id = 155;
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
        // Redstone Block -> Active Redstone Block
        case 152:
            *id = 153;
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
    float f = Mth::_random.genrand_int32();
    std::string name = "random.pop";
    level->playSound(src, name, 0.5, 0.4 / (f * 0.4 + 0.8));

    // Show the particles
    std::string part = "flame";
    float speed = small ? 0.15 : 0.3;
    y += small ? 0 : 0.5;
    for (float ym : {speed, -speed}) {
        // Sides
        for (int i = 0; i < 16; i++) {
            level->addParticle(part, x + 0.5, y, z + 0.5, (sin(i * (360/8))) * speed, (i > 8) * ym, (cos(i * (360/8))) * speed, 0);
        }
        // Top/bottom
        level->addParticle(part, x + 0.5, y, z + 0.5, 0, ym, 0, 0);
    }
}

bool transmutate_pedestal(Level *level, Player *player, int x, int y, int z, ItemInstance *item) {
    // Get
    if (level->getTile(x, y, z) != PEDESTAL_ID) return false;
    PedestalTileEntity *pedestal_te = (PedestalTileEntity *) custom_get<CustomTileEntity>(level->getTileEntity(x, y, z));
    if (!pedestal_te) return false;
    // Transmutate
    bool should_heal = false;
    ItemInstance &pitem = get_pedestal_item(pedestal_te);
    int id = pitem.id;
    int aux = transform_id(&id, pitem.auxiliary, &should_heal);
    // Set
    if (aux != -1 && (id != pitem.id || pitem.auxiliary != aux)) {
        pitem.id = id;
        pitem.auxiliary = aux;
        show_particles_and_pop(level, (Entity *) player, x, (float) y + 1.3f, z, true);
        // Heal
        if (should_heal && item) item->auxiliary = 0;
        return true;
    }
    return false;
}

struct NetherWandItem final : CustomItem {
    NetherWandItem(int id) : CustomItem(id) {}

    bool useOn(
        ItemInstance *item, Player *player, Level *level, int x, int y, int z, UNUSED int hit_side, UNUSED float hit_x, UNUSED float hit_y, UNUSED float hit_z
    ) override {
        // Deplete
        if (!player->abilities.infinite_items && item) {
            item->auxiliary -= 1;
        }

        // Let the transmutation begin
        int id = level->getTile(x, y, z);
        if (id == PEDESTAL_ID) {
            // Transmutate the pedestal
            return transmutate_pedestal(level, player, x, y, z, item);
        }
        // Transmutate the block
        int old_id = id, old_data = level->getData(x, y, z);
        int data = transform_id(&id, old_data, NULL);
        if (data == -1 || (old_id == id && old_data == data)) return 1;
        if (id < 256) {
            // Block
            level->setTileAndData(x, y, z, id, data);
        } else {
            // Item
            ItemInstance i = {.count = 1, .id = id, .auxiliary = data};
            ItemEntity *item_entity = (ItemEntity *) EntityFactory::CreateEntity(64, level);
            item_entity->constructor(level, x + 0.5f, y, z + 0.5f, i);
            item_entity->moveTo(x + 0.5f, y, z + 0.5f, 0, 0);
            level->addEntity((Entity *) item_entity);
            level->setTileAndData(x, y, z, 0, 0);
        }
        show_particles_and_pop(level, (Entity *) player, x, y, z, false);

        return 1;
    }

    void interactEnemy(UNUSED ItemInstance *item, Mob *mob) override {
        if (mob->getEntityTypeId() == 12) {
            // Zombify!
            Mob *zombpig = MobFactory::CreateMob(36, mob->level);
            zombpig->moveTo(mob->x, mob->y, mob->z, mob->yaw, mob->pitch);
            mob->level->addEntity((Entity *) zombpig);
            mob->remove();
            // Pop
            show_particles_and_pop(zombpig->level, (Entity *) zombpig, zombpig->x, zombpig->y, zombpig->z, false);
        }
    }
};

OVERWRITE_CALLS(GameMode_attack, void, GameMode_attack_injection, (GameMode_attack_t original, GameMode *self, Player *player, Entity *target)) {
    ItemInstance *held = player->inventory->getSelected();
    if (held && held->id == NETHER_WAND_ID) {
        held->auxiliary -= 1;
        target->fire_timer = std::max(target->fire_timer, 100);
    }
    return original(self, player, target);
}

// Create Item
static Item *nether_wand = NULL;
void make_nether_wand() {
    // Construct
    nether_wand = (new NetherWandItem(NETHER_WAND_ID - 256))->self;

    // Setup
    std::string name = "nether_wand";
    nether_wand->setDescriptionId(name);
    nether_wand->max_damage = 60;
    nether_wand->is_stacked_by_data = true;
    nether_wand->max_stack_size = 1;
    nether_wand->equipped = 1;
    nether_wand->category = 2;
    nether_wand->texture = NETHER_WAND_TEXTURE;
}
