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
static EntityTile *piston_moving = NULL;

struct MovingPistonTE : TileEntity {
    int moving_id;
    int moving_meta;
    int direction;
    bool extending;
    float pos = 0;
};

static MovingPistonTE *make_MovingPistonTE(
    int moving_id = 0,
    int moving_meta = 0,
    int direction = 0,
    bool extending = true
);
static void MovingPistonTE_place(MovingPistonTE *self);
static TileEntity_vtable *get_piston_te_vtable();

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
    if (id == 49 || id == 7 || t->destroyTime == -1) {
        return false;
    } else if (Tile_isEntityTile[id]) {
        return false;
    } else if (id == 29 || id == 33 || id == 36) {
        // Active pistons can't be pushed
        return level->vtable->getData(level, x, y, z) & 0b0111;
    }
    return true;
}

static int pis_dir[6][3] = {
    {0, -1, 0}, {0, 1, 0},
    {0, 0, -1}, {0, 0, 1},
    {-1, 0, 0}, {1, 0, 0}
};
static bool canPushLine(Level *level, int x, int y, int z, int direction) {
    direction &= 0b0111;
    int xo = x + pis_dir[direction][0];
    int yo = y + pis_dir[direction][1];
    int zo = z + pis_dir[direction][2];
    for (int i = 0; i < 13; i++) {
        if (yo <= 0 || 127 <= yo) {
            // OOB: Y
            return false;
        }
        int id = level->vtable->getTile(level, xo, yo, zo);
        if (id == 0) break;
        if (!pushable(level, xo, yo, zo, id)) {
            // TODO: Crushing
            return false;
        }
        // TODO: Breaking weak blocks
        // Too long
        if (i == 12) return false;
        // Keep going
        xo += pis_dir[direction][0];
        yo += pis_dir[direction][1];
        zo += pis_dir[direction][2];
    }
    return true;
}
static bool extend(Tile *self, Level *level, int x, int y, int z, int direction) {
    direction &= 0b0111;
    int xo = x + pis_dir[direction][0];
    int yo = y + pis_dir[direction][1];
    int zo = z + pis_dir[direction][2];
    for (int i = 0; i < 13; i++) {
        if (yo <= 0 || 127 <= yo) {
            // OOB: Y
            return false;
        }
        int id = level->vtable->getTile(level, xo, yo, zo);
        if (id == 0) break;
        if (!pushable(level, xo, yo, zo, id)) {
            return false;
        }
        // TODO: Breaking weak blocks
        // Too long
        if (i == 12) return false;
        // Keep going
        xo += pis_dir[direction][0];
        yo += pis_dir[direction][1];
        zo += pis_dir[direction][2];
    }
    // Move the blocks
    while (x != xo || y != yo || z != zo) {
        int bx = xo - pis_dir[direction][0];
        int by = yo - pis_dir[direction][1];
        int bz = zo - pis_dir[direction][2];
        int id = level->vtable->getTile(level, bx, by, bz);
        int meta = level->vtable->getData(level, bx, by, bz);
        if (id == self->id && bx == x && by == y && bz == z) {
            bool sticky = ((PistonBase *) self)->sticky;
            Level_setTileAndData(level, xo, yo, zo, 36, direction | (8 * sticky));
            // TODO: Piston head
            TileEntity *te = make_MovingPistonTE(1, direction | (sticky * 8), direction, true);
            Level_setTileEntity(level, xo, yo, zo, te);
        } else {
            Level_setTileAndData(level, xo, yo, zo, 36, meta);
            TileEntity *te = make_MovingPistonTE(id, meta, direction, true);
            Level_setTileEntity(level, xo, yo, zo, te);
        }
        xo = bx;
        yo = by;
        zo = bz;
    }
    return true;
}

static void move(Tile *self, Level *level, int x, int y, int z, bool extending, int data) {
    int direction = data & 0b0111;
    if (extending) {
        if (!extend(self, level, x, y, z, direction)) return;
        Level_setTileAndData(level, x, y, z, self->id, direction | 8);
        return;
    }
    // Retract
    int ox = x + pis_dir[direction][0];
    int oy = y + pis_dir[direction][1];
    int oz = z + pis_dir[direction][2];
    TileEntity *te = Level_getTileEntity(level, ox, oy, oz);
    if (te && te->vtable == get_piston_te_vtable()) {
        // Allow for retraction within a single tick
        MovingPistonTE_place((MovingPistonTE *) te);
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
        if (canPushLine(level, x, y, z, direction)) {
            Level_setTileAndData(level, x, y, z, self->id, direction | 8);
            move(self, level, x, y, z, true, direction);
        }
    } else if (!hasNeighborSignal && is_extended) {
        Level_setTileAndData(level, x, y, z, self->id, direction);
        move(self, level, x, y, z, false, direction);
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

// MovingPiston tileentity
static void MovingPistonTE_place(MovingPistonTE *self) {
    // Place
    if (self->level->vtable->getTile(self->level, self->x, self->y, self->z) == 36 && self->moving_id) {
        Level_setTileAndData(
            self->level,
            self->x, self->y, self->z,
            self->moving_id, self->moving_meta
        );
    }
    // Remove
    Level_removeTileEntity(self->level, self->x, self->y, self->z);
}

static void MovingPistonTE_tick(TileEntity *_self) {
    MovingPistonTE *self = (MovingPistonTE *) _self;
    if (self->pos >= 1) {
        MovingPistonTE_place(self);
        return;
    }
    self->pos = std::min(self->pos + 0.5f, 1.f);
}

static bool MovingPistonTE_shouldSave(UNUSED TileEntity *self) {
    return true;
}

static void MovingPistonTE_load(TileEntity *self, CompoundTag *tag) {
    std::string str = "move_id";
    ((MovingPistonTE *) self)->moving_id = CompoundTag_getShort(tag, &str);
    str = "move_meta";
    ((MovingPistonTE *) self)->moving_meta = CompoundTag_getShort(tag, &str);
    str = "direction";
    ((MovingPistonTE *) self)->direction = CompoundTag_getShort(tag, &str);
    str = "extending";
    ((MovingPistonTE *) self)->extending = CompoundTag_getShort(tag, &str);
}

static bool MovingPistonTE_save(TileEntity *self, CompoundTag *tag) {
    std::string str = "move_id";
    CompoundTag_putShort(tag, &str, ((MovingPistonTE *) self)->moving_id);
    str = "move_meta";
    CompoundTag_putShort(tag, &str, ((MovingPistonTE *) self)->moving_meta);
    str = "extending";
    CompoundTag_putShort(tag, &str, ((MovingPistonTE *) self)->extending);
    str = "direction";
    CompoundTag_putShort(tag, &str, ((MovingPistonTE *) self)->direction);
    return true;
}

CUSTOM_VTABLE(piston_te, TileEntity) {
    vtable->tick = MovingPistonTE_tick;
    vtable->shouldSave = MovingPistonTE_shouldSave;
    vtable->load = MovingPistonTE_load;
    vtable->save = MovingPistonTE_save;
}

static MovingPistonTE *make_MovingPistonTE(int moving_id, int moving_meta, int direction, bool extending) {
    MovingPistonTE *piston_te = new MovingPistonTE;
    ALLOC_CHECK(piston_te);
    TileEntity_constructor((TileEntity *) piston_te, 49);
    piston_te->renderer_id = 12;
    piston_te->vtable = get_piston_te_vtable();
    piston_te->moving_id = moving_id;
    piston_te->moving_meta = moving_meta;
    piston_te->extending = extending;
    piston_te->direction = direction;
    return piston_te;
}

HOOK_FROM_CALL(0xd2544, TileEntity *, TileEntityFactory_createTileEntity, (int id)) {
    if (id == 49) {
        return make_MovingPistonTE();
    }
    // Call original
    return TileEntityFactory_createTileEntity_original(id);
}

// MovingPiston entitytile
static TileEntity *MovingPiston_newTileEntity(UNUSED EntityTile *self) {
    return NULL;
}

static bool MovingPiston_mayPlace2(UNUSED EntityTile *self, UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) {
    // mayPlace doesn't need to be overrode because Tile_mayPlace calls mayPlace2
    return false;
}

static void MovingPiston_onRemove(UNUSED EntityTile *self, Level *level, int x, int y, int z) {
    // TODO: TileEntityPistonMoving
    TileEntity *te = Level_getTileEntity(level, x, y, z);
    if (te != NULL) {
        MovingPistonTE_place((MovingPistonTE *) te);
    }
}

static int MovingPiston_getRenderShape(UNUSED EntityTile *self) {
    return -1;
}

static bool MovingPiston_isSolidRender(UNUSED EntityTile *self) {
    return 0;
}

static int MovingPiston_getRenderLayer(UNUSED EntityTile *self) {
    return 1;
}

static bool MovingPiston_isCubeShaped(UNUSED EntityTile *self) {
    return false;
}

static AABB *MovingPiston_getAABB(UNUSED EntityTile *self, UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) {
    return NULL;
}

static int MovingPiston_use(UNUSED EntityTile *self, Level *level, int x, int y, int z, UNUSED Player *player) {
    TileEntity *te = Level_getTileEntity(level, x, y, z);
    if (te != NULL) {
        MovingPistonTE_place((MovingPistonTE *) te);
        return 1;
    }
    return 0;
}

static void make_moving_piston(int id) {
    // TODO: wood mat/sound -> stone
    piston_moving = alloc_EntityTile();
    ALLOC_CHECK(piston_moving);
    int texture = INVALID_TEXTURE;
    Tile_constructor((Tile *) piston_moving, id, texture, Material_stone);
    Tile_isEntityTile[id] = true;
    piston_moving->texture = texture;

    // Set VTable
    piston_moving->vtable = dup_EntityTile_vtable(EntityTile_vtable_base);
    ALLOC_CHECK(piston_moving->vtable);
    piston_moving->vtable->newTileEntity = MovingPiston_newTileEntity;
    piston_moving->vtable->mayPlace2 = MovingPiston_mayPlace2;
    piston_moving->vtable->onRemove = MovingPiston_onRemove;
    piston_moving->vtable->getRenderShape = MovingPiston_getRenderShape;
    piston_moving->vtable->isSolidRender = MovingPiston_isSolidRender;
    piston_moving->vtable->getRenderLayer = MovingPiston_getRenderLayer;
    piston_moving->vtable->isCubeShaped = MovingPiston_isCubeShaped;
    piston_moving->vtable->getAABB = MovingPiston_getAABB;
    piston_moving->vtable->use = MovingPiston_use;

    // Init
    EntityTile_init(piston_moving);
    piston_moving->vtable->setDestroyTime(piston_moving, -1.0f);
    piston_moving->category = 4;
    std::string name = "pistonMoving";
    piston_moving->vtable->setDescriptionId(piston_moving, &name);
}

void make_pistons() {
    // Normal
    make_piston(33, false);
    // Sticky
    make_piston(29, true);
    // Head
    //make_piston_head(34);
    // Moving (it's a tile entity)
    make_moving_piston(36);
}
