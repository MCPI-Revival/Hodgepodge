#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "redstone.h"

// Create Item
static Item *redstone = NULL;
static Tile *redstone_wire = NULL;
static Item *repeater_item = NULL;
static Tile *inactive_repeater = NULL;
static Tile *active_repeater = NULL;
static Tile *redstone_block = NULL;

int RedStoneOreTile_getResource_injection(UNUSED Tile *t, UNUSED int data, UNUSED Random *random) {
    return REDSTONE_ID;
}

static void make_redstone_tileitems() {
    // Redstone dust
    redstone = (Item *) new TilePlanterItem();
    Item_constructor(redstone, REDSTONE_ID - 256);

    redstone->vtable = (Item_vtable *) TilePlanterItem_vtable_base;
    ((TilePlanterItem *) redstone)->tile_id = redstone_wire->id;
    std::string name = "redstoneDust";
    redstone->vtable->setDescriptionId(redstone, &name);
    redstone->category = 2;
    redstone->texture = 56;

    // Repeater
    repeater_item = (Item *) new TilePlanterItem();
    Item_constructor(repeater_item, REPEATER_ID - 256);

    repeater_item->vtable = (Item_vtable *) TilePlanterItem_vtable_base;
    ((TilePlanterItem *) repeater_item)->tile_id = inactive_repeater->id;
    name = "repeater";
    repeater_item->vtable->setDescriptionId(repeater_item, &name);
    repeater_item->category = 2;
    repeater_item->texture = 86;
}

// Redstone tile
static bool RedstoneWire_isSignalSource(UNUSED Tile *self) {
    return true;
}

static AABB *RedstoneWire_getAABB(UNUSED Tile *self, UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) {
    return NULL;
}
static bool RedstoneWire_isSolidRender(UNUSED Tile *self) {
    return false;
}
static bool RedstoneWire_isCubeShaped(UNUSED Tile *self) {
    return false;
}
static int RedstoneWire_getRenderShape(UNUSED Tile *self) {
    return 52;
}
static bool RedstoneWire_mayPlace2(UNUSED Tile *self, Level *level, int x, int y, int z) {
    return level->vtable->isSolidRenderTile(level, x, y - 1, z);
}

static int RedstoneWire_getRenderLayer(UNUSED Tile *self) {
    return 1;
}

static int RedstoneWire_getResourceCount(UNUSED Tile *self, UNUSED Random *random) {
    return 1;
}

static int RedstoneWire_getColor(UNUSED Tile *self, LevelSource *level_source, int x, int y, int z) {
    int data = level_source->vtable->getData(level_source, x, y, z) * 7;
    if (data == 0) return 0x660000;
    return 0x880000 + (data << 16);
}

static int RedstoneWire_maxToCur(Level *level, int x, int y, int z, int other) {
    if (level->vtable->getTile(level, x, y, z) != 55) {
        return other;
    } else {
        int data = level->vtable->getData(level, x, y, z);
        return data > other ? data : other;
    }
}

static std::vector<Vec3> wire_neighbors = {};
static bool checking_power = false;
static void RedstoneWire_update(Level *level, int x, int y, int z, int x2, int y2, int z2) {
    int data = level->vtable->getData(level, x, y, z);
    int power = 0;
    checking_power = true;
    bool powered = Level_hasNeighborSignal(level, x, y, z);
    checking_power = false;
    if (powered) {
        power = 15;
    } else {
        for (int side = 0; side < 4; side++) {
            // Check each side
            int xo = x, zo = z;
            if (side == 0) xo--;
            else if (side == 1) xo++;
            else if (side == 2) zo--;
            else if (side == 3) zo++;

            // Get the power
            if (xo != x2 || y != y2 || zo != z2) {
                // Next to
                power = RedstoneWire_maxToCur(level, xo, y, zo, power);
            }
            if (
                level->vtable->isSolidRenderTile(level, xo, y, zo)
                && !level->vtable->isSolidRenderTile(level, x, y + 1, z)
            ) {
                // Diagonal, +y
                if (xo != x2 || y + 1 != y2 || zo != z2) {
                    power = RedstoneWire_maxToCur(level, xo, y + 1, zo, power);
                }
            } else if (
                !level->vtable->isSolidRenderTile(level, xo, y, zo)
                && (xo != x2 || y - 1 != y2 || zo != z2)
            ) {
                // Diagonal, -y
                power = RedstoneWire_maxToCur(level, xo, y - 1, zo, power);
            }
        }
        if (power > 0) {
            // The power shrinks as it goes on
            --power;
        } else {
            power = 0;
        }
    }
    // Update
    if (power != data) {
        // Set data
        //level->no_update = true;
        Level_setData(level, x, y, z, power);
        //Level_setTileDirty(level, x, y, z);
        //level->no_update = false;

        for (int side = 0; side < 4; side++) {
            // Check each side
            int xo = x, yo = y - 1, zo = z;
            if (side == 0) xo--;
            else if (side == 1) xo++;
            else if (side == 2) zo--;
            else if (side == 3) zo++;

            if (level->vtable->isSolidRenderTile(level, xo, y, zo)) {
                yo += 2;
            }
            int cur = RedstoneWire_maxToCur(level, xo, y, zo, -1);
            power = level->vtable->getData(level, x, y, z);
            if (power > 0) power--;
            if (cur >= 0 && cur != power) {
                RedstoneWire_update(level, xo, y, zo, x, y, z);
            }

            cur = RedstoneWire_maxToCur(level, xo, yo, zo, -1);
            power = level->vtable->getData(level, x, y, z);
            if (power > 0) power--;
            if (cur >= 0 && cur != power) {
                RedstoneWire_update(level, xo, yo, zo, x, y, z);
            }
        }

        if (power == 0 || data == 0) {
            wire_neighbors.push_back(Vec3{(float) x,     (float) y,     (float) z    });
            wire_neighbors.push_back(Vec3{(float) x - 1, (float) y,     (float) z    });
            wire_neighbors.push_back(Vec3{(float) x + 1, (float) y,     (float) z    });
            wire_neighbors.push_back(Vec3{(float) x,     (float) y - 1, (float) z    });
            wire_neighbors.push_back(Vec3{(float) x,     (float) y + 1, (float) z    });
            wire_neighbors.push_back(Vec3{(float) x,     (float) y,     (float) z - 1});
            wire_neighbors.push_back(Vec3{(float) x,     (float) y,     (float) z + 1});
        }
    }
}

static void RedstoneWire_propagate(Level *level, int x, int y, int z) {
    RedstoneWire_update(level, x, y, z, x, y, z);
    std::vector<Vec3> neighbors = wire_neighbors;
    wire_neighbors = {};
    for (const Vec3 &i : neighbors) {
        Level_updateNearbyTiles(level, i.x, i.y, i.z, 55);
    }
}

static void RedstoneWire_neighborChanged(Tile *self, Level *level, int x, int y, int z, UNUSED int neighborId) {
    if (!RedstoneWire_mayPlace2(self, level, x, y, z)) {
        Level_setTile(level, x, y, z, 0);
        // TODO drop on ground
        return;
    } else {
        RedstoneWire_propagate(level, x, y, z);
    }
}

static void updateWires(Level *level, int x, int y, int z) {
    if (level->vtable->getTile(level, x, y, z) != 55) return;
    Level_updateNearbyTiles(level, x,     y,     z,     55);
    Level_updateNearbyTiles(level, x - 1, y,     z,     55);
    Level_updateNearbyTiles(level, x + 1, y,     z,     55);
    Level_updateNearbyTiles(level, x,     y - 1, z,     55);
    Level_updateNearbyTiles(level, x,     y + 1, z,     55);
    Level_updateNearbyTiles(level, x,     y,     z - 1, 55);
    Level_updateNearbyTiles(level, x,     y,     z + 1, 55);
}

#define UPDATE_IF_SOLID(x2, y2, z2) \
    if (level->vtable->isSolidRenderTile(level, (x2), (y2), (z2))) { \
        updateWires(level, (x2), (y2) + 1, (z2)); \
    } else { \
        updateWires(level, (x2), (y2) - 1, (z2)); \
    }
void RedstoneWire_onPlaceOrRemove(UNUSED Tile *self, Level *level, int x, int y, int z) {
    RedstoneWire_propagate(level, x, y, z);
    Level_updateNearbyTiles(level, x, y + 1, z, 55);
    Level_updateNearbyTiles(level, x, y - 1, z, 55);
    updateWires(level, x - 1, y, z);
    updateWires(level, x + 1, y, z);
    updateWires(level, x, y, z - 1);
    updateWires(level, x, y, z + 1);
    UPDATE_IF_SOLID(x - 1, y, z);
    UPDATE_IF_SOLID(x + 1, y, z);
    UPDATE_IF_SOLID(x, y, z - 1);
    UPDATE_IF_SOLID(x, y, z + 1);
}
#undef UPDATE_IF_SOLID

bool canWireConnectTo(LevelSource *level, int x, int y, int z, int side) {
    int id = level->vtable->getTile(level, x, y, z);
    if (id == 55) {
        return true;
    } else if (id == 0) {
        return false;
    } else if (Tile_tiles[id]->vtable->isSignalSource(Tile_tiles[id])) {
        return true;
    } else if (id == inactive_repeater->id || id == active_repeater->id) {
        // Yessir, another special case!
        // Untested, as repeaters don't work yet
        int dir = level->vtable->getData(level, x, y, z);
        return dir == side || dir == (side ^ 1);
    }
    return false;
}

static bool RedstoneWire_getSignal2(UNUSED Tile *self, LevelSource *level, int x, int y, int z, int side) {
    if (checking_power) return false;
    if (level->vtable->getData(level, x, y, z) == 0) return false;
    // The block under it is always powered

    if (side == 1) return true;
    bool on_xm = (canWireConnectTo(level, x - 1, y, z, 1) || (!level->vtable->isSolidRenderTile(level, x - 1, y, z) && canWireConnectTo(level, x - 1, y - 1, z, -1)));
    bool on_xp = (canWireConnectTo(level, x + 1, y, z, 3) || (!level->vtable->isSolidRenderTile(level, x + 1, y, z) && canWireConnectTo(level, x + 1, y - 1, z, -1)));
    bool on_zm = (canWireConnectTo(level, x, y, z - 1, 2) || (!level->vtable->isSolidRenderTile(level, x, y, z - 1) && canWireConnectTo(level, x, y - 1, z - 1, -1)));
    bool on_zp = (canWireConnectTo(level, x, y, z + 1, 0) || (!level->vtable->isSolidRenderTile(level, x, y, z + 1) && canWireConnectTo(level, x, y - 1, z + 1, -1)));
    if (!level->vtable->isSolidRenderTile(level, x, y + 1, z)) {
        if (level->vtable->isSolidRenderTile(level, x - 1, y, z) && canWireConnectTo(level, x - 1, y + 1, z, -1)) on_xm = true;
        if (level->vtable->isSolidRenderTile(level, x + 1, y, z) && canWireConnectTo(level, x + 1, y + 1, z, -1)) on_xp = true;
        if (level->vtable->isSolidRenderTile(level, x, y, z - 1) && canWireConnectTo(level, x, y + 1, z - 1, -1)) on_zm = true;
        if (level->vtable->isSolidRenderTile(level, x, y, z + 1) && canWireConnectTo(level, x, y + 1, z + 1, -1)) on_zp = true;
    }
    if (!on_zm && !on_xp && !on_xm && !on_zp && side >= 2 && side <= 5) return true;
    if (side == 2 && on_zm && !on_xm && !on_xp) return true;
    if (side == 3 && on_zp && !on_xm && !on_xp) return true;
    if (side == 4 && on_xm && !on_zm && !on_zp) return true;
    if (side == 5 && on_xp && !on_zm && !on_zp) return true;
    return false;
}

static bool RedstoneWire_getDirectSignal(Tile *self, Level *level, int x, int y, int z, int direction) {
    return checking_power ? false : RedstoneWire_getSignal2(self, (LevelSource *) level, x, y, z, direction);
}

static void make_repeater(int id);
void make_redstone_wire() {
    // Redstone blocks
    redstone_wire = new Tile();
    ALLOC_CHECK(redstone_wire);
    int texture = 164;
    Tile_constructor(redstone_wire, 55, texture, Material_glass);
    redstone_wire->texture = texture;

    // Set VTable
    redstone_wire->vtable = dup_Tile_vtable(Tile_vtable_base);
    ALLOC_CHECK(redstone_wire->vtable);
    redstone_wire->vtable->isSignalSource = RedstoneWire_isSignalSource;
    //redstone_wire->vtable->getSignal = RedstoneWire_getSignal;
    redstone_wire->vtable->getAABB = RedstoneWire_getAABB;
    redstone_wire->vtable->getSignal2 = RedstoneWire_getSignal2;
    redstone_wire->vtable->getDirectSignal = RedstoneWire_getDirectSignal;
    redstone_wire->vtable->getRenderShape = RedstoneWire_getRenderShape;
    redstone_wire->vtable->getRenderLayer = RedstoneWire_getRenderLayer;
    redstone_wire->vtable->mayPlace2 = RedstoneWire_mayPlace2;
    redstone_wire->vtable->getColor = RedstoneWire_getColor;
    redstone_wire->vtable->isCubeShaped = RedstoneWire_isCubeShaped;
    redstone_wire->vtable->isSolidRender = RedstoneWire_isSolidRender;
    redstone_wire->vtable->neighborChanged = RedstoneWire_neighborChanged;
    redstone_wire->vtable->getResource = RedStoneOreTile_getResource_injection;
    redstone_wire->vtable->getResourceCount = RedstoneWire_getResourceCount;
    redstone_wire->vtable->onPlace = RedstoneWire_onPlaceOrRemove;
    redstone_wire->vtable->onRemove = RedstoneWire_onPlaceOrRemove;

    // Init
    Tile_init(redstone_wire);
    redstone_wire->vtable->setDestroyTime(redstone_wire, 0.0f);
    redstone_wire->vtable->setExplodeable(redstone_wire, 0.0f);
    redstone_wire->category = 4;
    std::string name = "redstone";
    redstone_wire->vtable->setDescriptionId(redstone_wire, &name);
    redstone_wire->vtable->setShape(redstone_wire, 0.0f, 0.0f, 0.0f, 1.0f, 0.0625f, 1.0f);

    make_repeater(93);
    make_repeater(94);
    make_redstone_tileitems();
}

// Repeater
static AABB *Repeater_getAABB(UNUSED Tile *self, UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) {
    // This is wrong, but I don't care
    return NULL;
}
static bool Repeater_isSolidRender(UNUSED Tile *self) {
    return false;
}
static bool Repeater_isCubeShaped(UNUSED Tile *self) {
    return false;
}
static bool Repeater_mayPlace2(UNUSED Tile *self, Level *level, int x, int y, int z) {
    return level->vtable->isSolidRenderTile(level, x, y - 1, z);
}

static void Repeater_neighborChanged(Tile *self, Level *level, int x, int y, int z, UNUSED int neighborId) {
    if (!Repeater_mayPlace2(self, level, x, y, z)) {
        Level_setTile(level, x, y, z, 0);
        // TODO drop on ground
        return;
    }
}

static bool ActiveRepeater_getSignal2(UNUSED Tile *self, LevelSource *level, int x, int y, int z, int side) {
    int dir = level->vtable->getData(level, x, y, z) & 0b0011;
    return (dir == 0 && side == 3)
        || (dir == 1 && side == 4)
        || (dir == 2 && side == 2)
        || (dir == 3 && side == 5);
}

static void make_repeater(int id) {
    // Redstone blocks
    Tile *repeater = new Tile();
    ALLOC_CHECK(repeater);
    int texture = 131 + (id == 94)*16;
    Tile_constructor(repeater, id, texture, Material_stone);
    repeater->texture = texture;

    // Set VTable
    repeater->vtable = dup_Tile_vtable(Tile_vtable_base);
    ALLOC_CHECK(repeater->vtable);
    // BLUF: Repeaters are a hardcoded special case and I'm mad
    // "Something doesn't fit with the current system? Hardcode a special case
    // Too lazy to add a virtual method? Hardcode a special case
    // It's easier to ignore mounting tech debt? Hardcode a special case
    // Can't remember how it should work? You better believe it, hardcode a special case!"
    // - Mojank design principles
    //repeater->vtable->isSignalSource = Repeater_isSignalSource;
    if (id == 94) {
        repeater->vtable->getSignal2 = ActiveRepeater_getSignal2;
    }
    repeater->vtable->getAABB = Repeater_getAABB;
    repeater->vtable->mayPlace2 = Repeater_mayPlace2;
    repeater->vtable->isCubeShaped = Repeater_isCubeShaped;
    repeater->vtable->isSolidRender = Repeater_isSolidRender;
    repeater->vtable->neighborChanged = Repeater_neighborChanged;
    // TODO: Repeater item
    //repeater->vtable->getResource = Repeater_getResource;

    // Init
    Tile_init(repeater);
    repeater->vtable->setDestroyTime(repeater, 0.0f);
    repeater->vtable->setExplodeable(repeater, 0.0f);
    repeater->category = 4;
    std::string name = "repeater";
    repeater->vtable->setDescriptionId(repeater, &name);
    repeater->vtable->setShape(repeater, 0.0f, 0.0f, 0.0f, 1.0f, 0.0625f, 1.0f);

    (id == 93 ? inactive_repeater : active_repeater) = repeater;
}


// Redstone block
static bool RedstoneBlock_isSignalSource(UNUSED Tile *self) {
    return true;
}

static bool RedstoneBlock_getSignal(UNUSED Tile *self, UNUSED LevelSource *level, UNUSED int x, UNUSED int y, UNUSED int z) {
    return true;
}

static bool RedstoneBlock_getSignal2(UNUSED Tile *self, UNUSED LevelSource *level, UNUSED int x, UNUSED int y, UNUSED int z, UNUSED int direction) {
    return true;
}

void make_redstone_block() {
    // Redstone blocks
    redstone_block = new Tile();
    ALLOC_CHECK(redstone_block);
    int texture = 13+16*7;
    Tile_constructor(redstone_block, 152, texture, Material_glass);
    redstone_block->texture = texture;

    // Set VTable
    redstone_block->vtable = dup_Tile_vtable(Tile_vtable_base);
    ALLOC_CHECK(redstone_block->vtable);
    redstone_block->vtable->isSignalSource = RedstoneBlock_isSignalSource;
    redstone_block->vtable->getSignal = RedstoneBlock_getSignal;
    redstone_block->vtable->getSignal2 = RedstoneBlock_getSignal2;

    // Init
    Tile_init(redstone_block);
    redstone_block->vtable->setDestroyTime(redstone_block, 2.0f);
    redstone_block->vtable->setExplodeable(redstone_block, 10.0f);
    redstone_block->vtable->setSoundType(redstone_block, &Tile_SOUND_STONE);
    redstone_block->category = 4;
    std::string name = "redstone_block";
    redstone_block->vtable->setDescriptionId(redstone_block, &name);
}

bool Level_getSignal_isSolidBlockingTile_injection(Level *level, int x, int y, int z) {
    int id = level->vtable->getTile(level, x, y, z);
    if (id == 152) return false;
    // Call original
    return level->vtable->isSolidBlockingTile(level, x, y, z);
}

__attribute__((constructor)) static void init() {
    // Make the ore drop it
    patch_address((void *) 0x113a38, (void *) RedStoneOreTile_getResource_injection);
    // Fix redstone blocks providing signals
    overwrite_call((void *) 0xa5e8c, (void *) Level_getSignal_isSolidBlockingTile_injection);
}
