#include <cmath>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>

#include "api.h"
#include "belt.h"

/// The less ugly code
static Tile *conveyorbelt = NULL;

// Item Textures
static int ConveyorBelt_getTexture2(UNUSED Tile *tile, UNUSED int face, UNUSED int data) {
    return BELT_ITEM_TEXTURE;
}

// Rendering
static bool ConveyorBelt_isSolidRender(UNUSED Tile *tile) {
    return 0;
}
static int ConveyorBelt_getRenderLayer(UNUSED Tile *tile) {
    return 1;
}
static bool ConveyorBelt_isCubeShaped(UNUSED Tile *tile) {
    return false;
}
static int ConveyorBelt_getRenderShape(UNUSED Tile *tile) {
    return 47;
}

// Size
static void ConveyorBelt_updateDefaultShape(Tile *tile) {
    // Set the default shape
    tile->vtable->setShape(
        tile,
        0.0, 0.0, 0.0,
        1.0, 0.5, 1.0
    );
}

static AABB *ConveyorBelt_getAABB(Tile *tile, UNUSED Level *level, int x, int y, int z) {
    // Corner 1
    AABB *aabb = &tile->aabb;
    aabb->x1 = (float) x;
    aabb->y1 = (float) y;
    aabb->z1 = (float) z;

    // Corner 2
    aabb->x2 = (float) x + 1.0;
    aabb->y2 = (float) y + 0.5;
    aabb->z2 = (float) z + 1.0;

    return aabb;
}

static void ConveyorBelt_updateShape(Tile *tile, UNUSED LevelSource *level, UNUSED int x, UNUSED int y, UNUSED int z) {
    ConveyorBelt_updateDefaultShape(tile);
}

// Direction
static void ConveyorBelt_setPlacedBy(Tile *tile, Level *level, int x, int y, int z, Mob *placer) {
    int face = (int) std::floor((placer->yaw * 4.0) / 360.0 + 0.5) & 3;
    if (face == 0) {
        face = 2;
    } else if (face == 1) {
        face = 5;
    } else if (face == 2) {
        face = 3;
    } else if (face == 3) {
        face = 4;
    } else {
        face = 0;
    }
    Level_setTileAndData(level, x, y, z, tile->id, face);
}

// Interaction
static int ConveyorBelt_use(UNUSED Tile *tile, Level *level, int x, int y, int z, Player *player) {
    AABB aabb{(float) x, (float) y, (float) z, (float) x, (float) y, (float) z};
    std::vector<Entity *> buff = {};
    int num_removed = 0;
    if (!Level_getEntitiesOfType(level, 64, &aabb, &buff)) return false;
    // Remove items and give them to player
    for (Entity *e : buff) {
        // TODO: Check MP support
        if ((int)e->x == x && (int)e->y == y && (int)e->z == z) {
            num_removed += 1;
            Inventory *inventory = player->inventory;
            if (inventory->vtable->add(inventory, &((ItemEntity*) e)->item)) {
                // Don't remove if it wasn't given fully to the player
                e->vtable->remove(e);
            }
        }
    }
    return num_removed != 0;
}

void ConveyorBelt_entityInside(UNUSED Tile *tile, Level *level, int x, int y, int z, Entity *entity) {
    constexpr float mov_speed = 0.1f;
    int data = level->vtable->getData(level, x, y, z);
    if (data == 2) {
        entity->vel_z = std::min(entity->vel_z, -mov_speed);
    } else if (data == 3) {
        entity->vel_z = std::max(entity->vel_z, mov_speed);
    } else if (data == 4) {
        entity->vel_x = std::min(entity->vel_x, -mov_speed);
    } else if (data == 5) {
        entity->vel_x = std::max(entity->vel_x, mov_speed);
    } else {
        // We do a little trolling
        entity->vel_y = std::max(entity->vel_y, 0.5f);
    }
    // Don't let items despawn on belts
    if (entity->vtable->getEntityTypeId(entity) == 64) {
        ((ItemEntity *) entity)->age = 0;
    }
}

void ItemEntity_playerTouch_injection(ItemEntity *self, Player *player) {
    int t = self->level->vtable->getTile(self->level, self->x, self->y, self->z);
    // Disallow item pickup if it's on a belt or going on to one
    if (t != BELT_ID) {
        ItemEntity_playerTouch_non_virtual(self, player);
    }
}

__attribute__((constructor)) static void init() {
    patch_address((void *) ItemEntity_playerTouch_vtable_addr, (void *) ItemEntity_playerTouch_injection);
}

// Makes the conveyorbelts
void make_conveyorbelt() {
    // Construct
    conveyorbelt = new Tile();
    ALLOC_CHECK(conveyorbelt);
    int texture = BELT_ITEM_TEXTURE;
    Tile_constructor(conveyorbelt, BELT_ID, texture, Material_metal);
    conveyorbelt->texture = texture;
    ConveyorBelt_updateDefaultShape(conveyorbelt);

    // Set VTable
    conveyorbelt->vtable = dup_Tile_vtable(Tile_vtable_base);
    ALLOC_CHECK(conveyorbelt->vtable);

    // Modify functions
    conveyorbelt->vtable->getTexture2 = ConveyorBelt_getTexture2;
    conveyorbelt->vtable->isSolidRender = ConveyorBelt_isSolidRender;
    conveyorbelt->vtable->getRenderLayer = ConveyorBelt_getRenderLayer;
    conveyorbelt->vtable->isCubeShaped = ConveyorBelt_isCubeShaped;
    conveyorbelt->vtable->getRenderShape = ConveyorBelt_getRenderShape;
    conveyorbelt->vtable->updateShape = ConveyorBelt_updateShape;
    conveyorbelt->vtable->updateDefaultShape = ConveyorBelt_updateDefaultShape;
    conveyorbelt->vtable->getAABB = ConveyorBelt_getAABB;
    conveyorbelt->vtable->setPlacedBy = ConveyorBelt_setPlacedBy;
    conveyorbelt->vtable->entityInside = ConveyorBelt_entityInside;
    conveyorbelt->vtable->use = ConveyorBelt_use;

    // Init
    Tile_init(conveyorbelt);
    conveyorbelt->vtable->setDestroyTime(conveyorbelt, 2.0f);
    conveyorbelt->vtable->setExplodeable(conveyorbelt, 10.0f);
    conveyorbelt->category = 4;
    std::string name = "ConveyorBelt";
    conveyorbelt->vtable->setDescriptionId(conveyorbelt, &name);
}
