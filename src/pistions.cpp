#include <cmath>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>

#include "piston.h"
#include "api.h"
#include "init.h"

struct PistonBase : Tile {
    bool sticky;
};

// Am is -A and Ap is +A
// a ^ 1 can be used to swap the direction of a
enum Side {
    Ym = 0,
    Yp,
    Zm,
    Zp,
    Xm,
    Xp,
};

static Tile *piston_base = NULL;
static Tile *sticky_piston_base = NULL;
UNUSED static Tile *piston_head = NULL;
UNUSED static Entity *piston_moving = NULL;

static bool PistonBase_isSolidRender(UNUSED Tile *self) {
    return false;
}

static void PistonBase_setPlacedBy(Tile *self, Level *level, int x, int y, int z, Mob *placer) {
    int face = (int) std::floor((placer->yaw * 4.0) / 360.0 + 0.5) & 3;
    if (face == 0) {
        face = 2;
    } else if (face == 1) {
        face = 5;
    } else if (face == 2) {
        face = 3;
    } else if (face == 3) {
        face = 4;
    }
    // Check up/down
    if (placer->pitch > 55) face = 1;
    else if (placer->pitch < -55) face = 0;
    Level_setTileAndData(level, x, y, z, self->id, face);
}

static int PistonBase_getTexture3(Tile *self, UNUSED LevelSource *levelsrc, int x, int y, int z, int face) {
    int data = mc->level->vtable->getData(mc->level, x, y, z);
    bool is_extended = data & 0b1000;
    data &= 0b0111;
    if (data == face || data == 7) {
        if (is_extended && data != 7) {
            return 13+16*6;
        }
        if (((PistonBase *) self)->sticky) {
            return 10+16*6;
        } else {
            return 11+16*6;
        }
    } else if ((data ^ 1) == face) {
        // Reverse side
        return 13+16*6;
    }
    // Normal side
    return 12+16*6;
}

static bool getNeighborSignal(Level *level, int x, int y, int z, int direction) {
    return (direction != 0 && Level_getSignal(level, x, y - 1, z, 0))
        || (direction != 1 && Level_getSignal(level, x, y + 1, z, 1))
        || (direction != 2 && Level_getSignal(level, x, y, z - 1, 2))
        || (direction != 3 && Level_getSignal(level, x, y, z + 1, 3))
        || (direction != 4 && Level_getSignal(level, x - 1, y, z, 4))
        || (direction != 5 && Level_getSignal(level, x + 1, y, z, 5))
        || Level_getSignal(level, x, y, z, 0)
#ifdef USE_QC
        || Level_getSignal(level, x, y + 2, z, 1)
        || Level_getSignal(level, x, y + 1, z - 1, 2)
        || Level_getSignal(level, x, y + 1, z + 1, 3)
        || Level_getSignal(level, x - 1, y + 1, z, 4)
        || Level_getSignal(level, x + 1, y + 1, z, 5)
#endif
    ;
}

static bool pushable(Level *level, int x, int y, int z, int id) {
    if (id == 0) return true;
    Tile *t = Tile_tiles[id];
    if (id == 49 || id == 7 || t->destroy_time == -1) {
        return false;
    } else if (Tile_isEntityTile[id]) {
        return false;
    } else if (id == 29 || id == 33) {
        return level->vtable->getData(level, x, y, z) & 0b0111;
    }
}

static int pis_dir[3][6] = {
    {0, -1, 0}, {0, 1, 0},
    {0, 0, -1}, {0, 0, 1},
    {-1, 0, 0}, {1, 0, 0}
}
static bool canPushLine(Level *level, int x, int y, int z, int direction) {
    int xo = x + pis_dir[direction & 0b0111][0];
    int yo = y + pis_dir[direction & 0b0111][2];
    int zo = z + pis_dir[direction & 0b0111][3];
    for (int i = 0; i < 13; i++) {
        if (yo <= 0 || 127 <= yo) {
            // OOB: Y
            return false;
        }
        int id = level->vtable->getTile(level, xo, yo, zo);
        if (id == 0) break;
        // TODO: Crushing <3
        if (!BlockPistonBase.isPushable(id, world, xo, yo, zo, true)) {
    }
}

static void PistonBase_neighborChanged(Tile *self, Level *level, int x, int y, int z, UNUSED int neighborId) {
    int data = level->vtable->getData(level, x, y, z);
    int direction = data & 0b0111;
    if (direction == 7) {
        // Oh no... it's a silly pison! D:
        return;
    }
    bool is_extended = data & 0b1000;
    bool hasNeighborSignal = getNeighborSignal(level, x, y, z, direction);
    if (hasNeighborSignal && !is_extended) {
        if (/*BlockPistonBase.canPushLine(world, x, y, z, direction)*/ 1) {
            Level_setTileAndData(level, x, y, z, self->id, direction | 8);
            // triggerEvent(x, y, z, 0, direction);
        }
    } else if (!hasNeighborSignal && is_extended) {
        Level_setTileAndData(level, x, y, z, self->id, direction);
        // triggerEvent(x, y, z, 1, direction);
    }
}

static void make_piston(int id, bool sticky) {
    // Construct
    Tile *piston = (Tile *) new PistonBase;
    ((PistonBase *) piston)->sticky = sticky;
    ALLOC_CHECK(piston);
    int texture = 0;
    Tile_constructor(piston, id, texture, Material_stone);
    piston->texture = texture;

    // Set VTable
    piston->vtable = dup_Tile_vtable(Tile_vtable_base);
    ALLOC_CHECK(piston->vtable);
    piston->vtable->isSolidRender = PistonBase_isSolidRender;
    piston->vtable->setPlacedBy = PistonBase_setPlacedBy;
    piston->vtable->getTexture3 = PistonBase_getTexture3;
    piston->vtable->neighborChanged = PistonBase_neighborChanged;

    // Init
    Tile_init(piston);
    piston->vtable->setDestroyTime(piston, 2.0f);
    piston->vtable->setExplodeable(piston, 10.0f);
    piston->vtable->setSoundType(piston, &Tile_SOUND_STONE);
    piston->category = 4;
    std::string name = !sticky ? "piston" : "pistonSticky";
    piston->vtable->setDescriptionId(piston, &name);

    if (sticky) {
        sticky_piston_base = piston;
    } else {
        piston_base = piston;
    }
}

void make_pistons() {
    // Normal
    make_piston(33, false);
    // Sticky
    make_piston(29, true);
    // Head
    //make_piston_head(34);
    // Moving
    //make_moving_piston(36);
}
