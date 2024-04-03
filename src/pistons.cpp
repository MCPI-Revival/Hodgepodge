#include <functional>
#include <cmath>

#include <GL/gl.h>

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

static void TileRenderer_renderTile_injection(TileRenderer *self, Tile *tile, int aux);
Tile *piston_base = NULL;
Tile *sticky_piston_base = NULL;
EntityTile *piston_moving = NULL;
Tile *piston_head = NULL;

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

static void PistonBase_neighborChanged(Tile *self, Level *level, int x, int y, int z, UNUSED int neighborId);
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
    Level_setData(level, x, y, z, face);
    PistonBase_neighborChanged(self, level, x, y, z, 0);
}

static int PistonBase_getTexture3(Tile *self, UNUSED LevelSource *levelsrc, int x, int y, int z, int face) {
    int data = mc->level->vtable->getData(mc->level, x, y, z);
    bool is_extended = data & 0b1000;
    data &= 0b0111;
    if (data == face || data == 7) {
        if (is_extended && data != 7) {
            return PISTON_HEAD_TEXTURE_EXTENDED;
        }
        if (((PistonBase *) self)->sticky) {
            return PISTON_HEAD_TEXTURE_STICKY;
        } else {
            return PISTON_HEAD_TEXTURE;
        }
    } else if ((data ^ 1) == face) {
        // Reverse side
        return PISTON_BACK_TEXTURE;
    }
    // Normal side
    return PISTON_SIDE_TEXTURE;
}

static bool getNeighborSignal(Level *level, int x, int y, int z, int direction) {
    return (direction != 0 && Level_getSignal(level, x, y - 1, z, 0))
        || (direction != 1 && Level_getSignal(level, x, y + 1, z, 1))
        || (direction != 2 && Level_getSignal(level, x, y, z - 1, 2))
        || (direction != 3 && Level_getSignal(level, x, y, z + 1, 3))
        || (direction != 4 && Level_getSignal(level, x - 1, y, z, 4))
        || (direction != 5 && Level_getSignal(level, x + 1, y, z, 5))
        || Level_getSignal(level, x, y, z, 0)
#ifdef P_USE_QC
        // Small tweak, pistons pointing up ignore QC on the block they are holding (such as a redstone block)
        || (direction != 1 && Level_getSignal(level, x, y + 2, z, 1))
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
    if (!t) return false;
    if (id == 49 || id == 7 || t->destroyTime == -1 || id == 36 || id == 34) {
        return false;
    } else if (Tile_isEntityTile[id]) {
        return false;
    } else if (id == 29 || id == 33) {
        // Active pistons can't be pushed
        bool active = level->vtable->getData(level, x, y, z) & 0b1000;
        return !active;
    } else if (t->vtable->getRenderShape(t) != 0) {
        // Temp thingy bc I'm too lazy to find out what weirdly shaped stuff can move
        // TODO: Remove temp thingy (bc I'm too lazy to find out what weirdly shaped stuff can move)
        return false;
    }
    return true;
}

static constexpr int pis_dir[6][3] = {
    {0, -1, 0}, {0, 1, 0},
    {0, 0, -1}, {0, 0, 1},
    {-1, 0, 0}, {1, 0, 0}
};
static bool canPushLine(Level *level, int x, int y, int z, int direction) {
    direction &= 0b0111;
    int xo = x + pis_dir[direction][0];
    int yo = y + pis_dir[direction][1];
    int zo = z + pis_dir[direction][2];
    for (int i = 0; i <= P_RANGE; i++) {
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
        if (i == P_RANGE) return false;
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
    for (int i = 0; i <= P_RANGE; i++) {
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
        if (i == P_RANGE) return false;
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
            TileEntity *te = make_MovingPistonTE(34, direction | (sticky * 8), direction, true);
            te->level = level;
            Level_setTileEntity(level, xo, yo, zo, te);
        } else {
            Level_setTileAndData(level, xo, yo, zo, 36, meta);
            TileEntity *te = make_MovingPistonTE(id, meta, direction, true);
            te->level = level;
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
        Level_setData(level, x, y, z, direction | 8);
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
    // Retract the piston
    Level_setTileAndData(level, x, y, z, 36, direction);
    te = make_MovingPistonTE(piston_head->id, direction | (8 * (self == sticky_piston_base)), direction, false);
    te->level = level;
    Level_setTileEntity(level, x, y, z, te);
    // Retract the block
    if (((PistonBase *) self)->sticky) {
        ox += pis_dir[direction][0];
        oy += pis_dir[direction][1];
        oz += pis_dir[direction][2];
        int id = level->vtable->getTile(level, ox, oy, oz);
        if (pushable(level, ox, oy, oz, id) && id != 0) {
            int data = level->vtable->getData(level, ox, oy, oz);
            ox -= pis_dir[direction][0];
            oy -= pis_dir[direction][1];
            oz -= pis_dir[direction][2];
            Level_setTileAndData(level, ox, oy, oz, 36, data);
            te = make_MovingPistonTE(id, data, direction, false);
            te->level = level;
            Level_setTileEntity(level, ox, oy, oz, te);
            ox += pis_dir[direction][0];
            oy += pis_dir[direction][1];
            oz += pis_dir[direction][2];
            Level_setTile(level, ox, oy, oz, 0);
        }
    }
}

void PistonBase_onRemove(UNUSED Tile *self, Level *level, int x, int y, int z) {
    int data = level->vtable->getData(level, x, y, z);
    int dir = data & 0b0111;
    if (dir == 0b111 || !(data & 0b1000)) return;
    int xo = x + pis_dir[dir][0];
    int yo = y + pis_dir[dir][1];
    int zo = z + pis_dir[dir][2];
    Level_setData(level, xo, yo, zo, 0);
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
            Level_setData(level, x, y, z, direction | 8);
            move(self, level, x, y, z, true, direction);
        }
    } else if (!hasNeighborSignal && is_extended) {
        Level_setData(level, x, y, z, direction);
        move(self, level, x, y, z, false, direction);
    }
}

static int PistonBase_getRenderShape(UNUSED Tile *self) {
    return 50;
}

AABB *PistonBase_getAABB(Tile *self, UNUSED Level *level, int x, int y, int z) {
    //int data = level->vtable->getData(level, x, y, z);
    //int dir = data & 0b0111, on = data & 0b1000;
    AABB *aabb = &self->aabb;
    aabb->x1 = x;
    aabb->y1 = y;
    aabb->z1 = z;
    aabb->x2 = x + 1.f;
    aabb->y2 = y + 1.f;
    aabb->z2 = z + 1.f;
    /*if (on && data != 0b1111) {
        if (dir == 0) {
            aabb->y1 += 0.1;
        } else if (dir == 1) {
            aabb->y2 -= 0.1;
        }
    }
    // Set shape, it's wrong but why not?
    self->x1 = aabb->x1;
    self->y1 = aabb->y1;
    self->z1 = aabb->z1;
    self->x2 = aabb->x2;
    self->y2 = aabb->y2;
    self->z2 = aabb->z2;*/
    return aabb;
}

static bool PistonBase_isCubeShaped(UNUSED Tile *self) {
    return false;
}

static void make_piston(int id, bool sticky) {
    // Construct
    Tile *piston = (Tile *) new PistonBase;
    ((PistonBase *) piston)->sticky = sticky;
    ALLOC_CHECK(piston);
    int texture = 0;
    Tile_constructor(piston, id, texture, Material_wood);
    piston->texture = texture;

    // Set VTable
    piston->vtable = dup_Tile_vtable(Tile_vtable_base);
    ALLOC_CHECK(piston->vtable);
    piston->vtable->isSolidRender = PistonBase_isSolidRender;
    piston->vtable->setPlacedBy = PistonBase_setPlacedBy;
    piston->vtable->getTexture3 = PistonBase_getTexture3;
    piston->vtable->neighborChanged = PistonBase_neighborChanged;
    piston->vtable->onRemove = PistonBase_onRemove;
    piston->vtable->getRenderShape = PistonBase_getRenderShape;
    piston->vtable->getAABB = PistonBase_getAABB;
    piston->vtable->isCubeShaped = PistonBase_isCubeShaped;

    // Init
    Tile_init(piston);
    piston->vtable->setDestroyTime(piston, 0.5f);
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
    if (!self->level) {
        return;
    }
    Level *level = self->level;
    int x = self->x, y = self->y, z = self->z, id = self->moving_id, meta = self->moving_meta;
    bool extending = self->extending;
    // Remove
    Level_removeTileEntity(level, x, y, z);
    // Place
    if (level->vtable->getTile(level, x, y, z) == 36 && id) {
        if (id == piston_head->id && !extending) {
            if (meta & 0b1000) {
                id = sticky_piston_base->id;
            } else {
                id = piston_base->id;
            }
            meta &= 0b0111;
        }
        Level_setTileAndData(
            level,
            x, y, z,
            id, meta
        );
        // Fix piston updating
        if (id == sticky_piston_base->id || id == piston_base->id)
        {
            // Update
            PistonBase_neighborChanged(Tile_tiles[id], level, x, y, z, 0);
        }
    }
}

static void MovingPistonTE_moveEntities(TileEntity *_self) {
    MovingPistonTE *self = (MovingPistonTE *) _self;
    // Get AABB
    AABB aabb = {
        (float) self->x,     (float) self->y,        (float) self->z,
        (float) self->x + 1, (float) self->y + 1.1f, (float) self->z + 1
    };
    float pos = 1 - self->pos;
    if (self->extending) pos = -pos;
    int dir = self->direction;
    aabb.x1 += pis_dir[dir][0] * pos;
    aabb.x2 += pis_dir[dir][0] * pos;
    aabb.y1 += pis_dir[dir][1] * pos;
    aabb.y2 += pis_dir[dir][1] * pos;
    aabb.z1 += pis_dir[dir][2] * pos;
    aabb.z2 += pis_dir[dir][2] * pos;

    // Get entities
    std::vector<Entity *> *entities = Level_getEntities(self->level, NULL, &aabb);
    float ox = pis_dir[dir][0] * P_SPEED;
    float oy = pis_dir[dir][1] * P_SPEED;
    float oz = pis_dir[dir][2] * P_SPEED;
    if (!self->extending) {
        ox = -ox; oy = -oy; oz = -oz;
    }
    for (Entity *entity : *entities) {
        if (entity) {
            entity->vtable->move(entity, ox, oy, oz);
        }
    }
}

static void MovingPistonTE_tick(TileEntity *_self) {
    // TODO: Fix piston clipping bug
    MovingPistonTE *self = (MovingPistonTE *) _self;
    int id = self->level->vtable->getTile(self->level, self->x, self->y, self->z);
    MovingPistonTE_moveEntities(_self);
    if (self->pos >= 1.f || id != 36) {
        MovingPistonTE_place(self);
        return;
    }
    self->pos = std::min(self->pos + P_SPEED, 1.f);
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
    TileEntity *te = Level_getTileEntity(level, x, y, z);
    if (te != NULL) {
        MovingPistonTE_place((MovingPistonTE *) te);
    }
}

static int MovingPiston_getRenderShape(UNUSED EntityTile *self) {
    return -1;
}

static bool MovingPiston_isSolidRender(UNUSED EntityTile *self) {
    return false;
}

static int MovingPiston_getRenderLayer(UNUSED EntityTile *self) {
    return 1;
}

static bool MovingPiston_isCubeShaped(UNUSED EntityTile *self) {
    return false;
}

static int MovingPiston_use(UNUSED EntityTile *self, Level *level, int x, int y, int z, UNUSED Player *player) {
    TileEntity *te = Level_getTileEntity(level, x, y, z);
    if (te != NULL) {
        te->level = level;
        MovingPistonTE_place((MovingPistonTE *) te);
    }
    return 1;
}

static void make_moving_piston(int id) {
    // TODO: wood sound -> stone
    piston_moving = new EntityTile();
    ALLOC_CHECK(piston_moving);
    int texture = INVALID_TEXTURE;
    Tile_constructor((Tile *) piston_moving, id, texture, Material_wood);
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
    piston_moving->vtable->use = MovingPiston_use;

    // Init
    EntityTile_init(piston_moving);
    piston_moving->vtable->setDestroyTime(piston_moving, -1.0f);
    piston_moving->category = 4;
    std::string name = "pistonMoving";
    piston_moving->vtable->setDescriptionId(piston_moving, &name);
}

// Piston head
void PistonHead_onRemove(UNUSED Tile *self, Level *level, int x, int y, int z) {
    int dir = level->vtable->getData(level, x, y, z) & 0b0111;
    if (dir == 0b111) return;
    int xo = x - pis_dir[dir][0];
    int yo = y - pis_dir[dir][1];
    int zo = z - pis_dir[dir][2];
    int id = level->vtable->getTile(level, xo, yo, zo);
    int data = level->vtable->getData(level, xo, yo, zo);
    if ((id == sticky_piston_base->id || id == piston_base->id) && data == (dir | 8)) {
        Level_setTile(level, xo, yo, zo, 0);
        // Add item
        ItemEntity *item_entity = (ItemEntity *) EntityFactory_CreateEntity(64, level);
        ALLOC_CHECK(item_entity);
        ItemInstance item = {.count = 1, .id = id, .auxiliary = 0};
        ItemEntity_constructor(item_entity, level, xo + 0.5f, yo, zo + 0.5f, &item);
        Entity_moveTo_non_virtual((Entity *) item_entity, xo + 0.5f, yo, zo + 0.5f, 0, 0);
        Level_addEntity(level, (Entity *) item_entity);
    }
}

static void PistonHead_neighborChanged(UNUSED Tile *self, Level *level, int x, int y, int z, UNUSED int neighborId) {
    int dir = level->vtable->getData(level, x, y, z) & 0b0111;
    if (dir == 0b111) return;
    int xo = x - pis_dir[dir][0];
    int yo = y - pis_dir[dir][1];
    int zo = z - pis_dir[dir][2];
    int id = level->vtable->getTile(level, xo, yo, zo);
    int data = level->vtable->getData(level, xo, yo, zo);
    if ((id != sticky_piston_base->id && id != piston_base->id) || data != (dir | 8)) {
        Level_setTile(level, x, y, z, 0);
    }
}

static void PistonHead_onPlace(Tile *self, Level *level, int x, int y, int z) {
    // Check
    PistonHead_neighborChanged(self, level, x, y, z, -1);
}

static int PistonHead_getTexture2(UNUSED Tile *t, UNUSED int face, int data) {
    return (data & 0b1000) ?
        // Sticky
        6*16+10:
        // Not sticky
        6*16+11;
}

#define AABB_BI 0.375f
#define AABB_BO (1 - AABB_BI)
#define AABB_HI 0.25f
#define AABB_HO (1 - AABB_HI)
static void PistonHead_setShapesCallback(Tile *self, int data, const std::function<void()> callback) {
    int dir = data & 0b0111;
    // TODO: Handle silly piston head
    if (dir == 0b111) return;
    switch (dir) {
        case 0b000:
            Tile_setShape_non_virtual(self, 0, 0, 0, 1.f, AABB_HI, 1.f);
            callback();
            Tile_setShape_non_virtual(self, AABB_BI, AABB_HI, AABB_BI, AABB_BO, 1.f, AABB_BO);
            callback();
            break;
        case 0b001:
            Tile_setShape_non_virtual(self, 0, AABB_HO, 0, 1.f, 1.f, 1.f);
            callback();
            Tile_setShape_non_virtual(self, AABB_BI, 0, AABB_BI, AABB_BO, AABB_HO, AABB_BO);
            callback();
            break;
        case 0b010:
            Tile_setShape_non_virtual(self, 0, 0, 0, 1.f, 1.f, AABB_HI);
            callback();
            Tile_setShape_non_virtual(self, AABB_BI, AABB_BI, AABB_HI, AABB_BO, AABB_BO, 1);
            callback();
            break;
        case 0b011:
            Tile_setShape_non_virtual(self, 0, 0, AABB_HO, 1.f, 1.f, 1.f);
            callback();
            Tile_setShape_non_virtual(self, AABB_BI, AABB_BI, 0, AABB_BO, AABB_BO, AABB_HO);
            callback();
            break;
        case 0b100:
            Tile_setShape_non_virtual(self, 0, 0, 0, AABB_HI, 1.f, 1.f);
            callback();
            Tile_setShape_non_virtual(self, AABB_HI, AABB_BI, AABB_BI, 1, AABB_BO, AABB_BO);
            callback();
            break;
        case 0b101:
            Tile_setShape_non_virtual(self, AABB_HO, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
            callback();
            Tile_setShape_non_virtual(self, 0, AABB_BI, AABB_BI, AABB_HO, AABB_BO, AABB_BO);
            callback();
            break;
        default:
            break;
    }
    Tile_setShape_non_virtual(self, 0, 0, 0, 1, 1, 1);
}
static void PistonHead_addAABBs(Tile *self, Level *level, int x, int y, int z, AABB *intersecting, std::vector<AABB> *aabbs) {
    int data = level->vtable->getData(level, x, y, z);
    PistonHead_setShapesCallback(
        self, data, [&]{
            Tile_addAABBs_non_virtual(self, level, x, y, z, intersecting, aabbs);
        }
    );
}

HOOK_FROM_CALL(0x47a58, bool, TileRenderer_tesselateInWorld, (TileRenderer *self, Tile *tile, int x, int y, int z)) {
    if (tile == piston_head) {
        int data = self->level->vtable->getData(self->level, x, y, z);
        PistonHead_setShapesCallback(
            tile, data, [&]{
                TileRenderer_tesselateBlockInWorld(self, tile, x, y, z);
            }
        );
        return 1;
    } else {
        return TileRenderer_tesselateInWorld_original(self, tile, x, y, z);
    }
}

HOOK_FROM_CALL(0x4ba0c, void, TileRenderer_renderTile, (TileRenderer *self, Tile *tile, int aux)) {
    if (tile == piston_head) {
        PistonHead_setShapesCallback(
            tile, aux, [&]{
                TileRenderer_renderTile_original(self, tile, aux);
            }
        );
    } else {
        TileRenderer_renderTile_original(self, tile, aux);
    }
}

static int PistonHead_getResource(UNUSED Tile *tile, UNUSED int data, UNUSED Random *random) {
    return 0;
}

static bool PistonHead_isCubeShaped(UNUSED Tile *tile) {
    return false;
}

static void make_piston_head(int id) {
    // Construct
    piston_head = new Tile();
    ALLOC_CHECK(piston_head);
    // TODO: Texture
    int texture = 6*16+11;
    Tile_constructor(piston_head, id, texture, Material_wood);
    piston_head->texture = texture;

    // Set VTable
    piston_head->vtable = dup_Tile_vtable(Tile_vtable_base);
    ALLOC_CHECK(piston_head->vtable);
    piston_head->vtable->isSolidRender = PistonBase_isSolidRender;
    piston_head->vtable->onRemove = PistonHead_onRemove;
    piston_head->vtable->onPlace = PistonHead_onPlace;
    piston_head->vtable->getTexture2 = PistonHead_getTexture2;
    piston_head->vtable->neighborChanged = PistonHead_neighborChanged;
    piston_head->vtable->addAABBs = PistonHead_addAABBs;
    piston_head->vtable->getResource = PistonHead_getResource;
    piston_head->vtable->isCubeShaped = PistonHead_isCubeShaped;

    // Init
    Tile_init(piston_head);
    piston_head->vtable->setDestroyTime(piston_head, 0.5f);
    piston_head->vtable->setSoundType(piston_head, &Tile_SOUND_STONE);
    piston_head->category = 4;
    std::string name = "piston_head";
    piston_head->vtable->setDescriptionId(piston_head, &name);
}

// Rendering moving piston
static void PistonTileEntityRenderer_render(UNUSED TileEntityRenderer *self, TileEntity *tileentity, float x, float y, float z, float unknown) {
    MovingPistonTE *piston_te = (MovingPistonTE *) tileentity;
    Tile *t = Tile_tiles[piston_te->moving_id];
    if (!t) return;
    float pos = (piston_te->pos - P_SPEED) + (P_SPEED * std::min(unknown, 1.f));
    float xo = (1 - pos) * pis_dir[piston_te->direction][0];
    float yo = (1 - pos) * pis_dir[piston_te->direction][1];
    float zo = (1 - pos) * pis_dir[piston_te->direction][2];
    if (!piston_te->extending) {
        xo = -xo;
        yo = -yo;
        zo = -zo;
    }

    glPushMatrix();
    glTranslatef(x + 0.5 - xo, y + 0.5 - yo, z + 0.5 - zo);

    std::string terrain = "terrain.png";
    EntityRenderer_bindTexture(NULL, &terrain);
    glDisable(GL_LIGHTING);

    TileRenderer *tr = ItemRenderer_tileRenderer;
    tr->level = (LevelSource *) piston_te->level;
    TileRenderer_renderTile_injection(tr, t, piston_te->moving_meta);

    if (t->id == piston_head->id && !piston_te->extending) {
        // Render piston base too
        glTranslatef(xo, yo, zo);
        Tile *t = 0;
        if (piston_te->moving_meta & 0b1000) {
            t = sticky_piston_base;
        } else {
            t = piston_base;
        }
        TileRenderer_renderTile_injection(tr, t, piston_te->moving_meta & 0b0111);
    }

    glPopMatrix();
}

CUSTOM_VTABLE(piston_ter, TileEntityRenderer) {
    vtable->render = PistonTileEntityRenderer_render;
}

static TileEntityRenderer *make_piston_tile_entity_renderer() {
    TileEntityRenderer *piston_ter = new TileEntityRenderer;
    ALLOC_CHECK(piston_ter);
    piston_ter->vtable = get_piston_ter_vtable();
    return piston_ter;
}

HOOK_FROM_CALL(0x67330, void, TileEntityRenderDispatcher_constructor, (TileEntityRenderDispatcher *self)) {
    // Call original
    TileEntityRenderDispatcher_constructor_original(self);
    // Add pedestal renderer
    TileEntityRenderer *pistonTileEntityRenderer = make_piston_tile_entity_renderer();
    self->renderer_map.insert(std::make_pair(12, pistonTileEntityRenderer));
}

void make_pistons() {
    // Normal
    make_piston(33, false);
    // Sticky
    make_piston(29, true);
    // Head
    make_piston_head(34);
    // Moving (it's a entity tile)
    make_moving_piston(36);
}
