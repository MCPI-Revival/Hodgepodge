#include <functional>
#include <cmath>

#include <GL/gl.h>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>

#include "piston.h"
#include "api.h"
#include "init.h"
#include "belt.h"

EXTEND_STRUCT(PistonBase, Tile, struct {
    bool sticky;
});

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

Tile *piston_base = NULL;
Tile *sticky_piston_base = NULL;
EntityTile *piston_moving = NULL;
Tile *piston_head = NULL;

EXTEND_STRUCT(MovingPistonTE, TileEntity, struct {
    int moving_id;
    int moving_meta;
    int direction;
    bool extending;
    float pos = 0;
});

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
    level->setData(x, y, z, face);
    PistonBase_neighborChanged(self, level, x, y, z, 0);
}

static int PistonBase_getTexture3(Tile *self, UNUSED LevelSource *levelsrc, int x, int y, int z, int face) {
    int data = mc->level->getData(x, y, z);
    bool is_extended = data & 0b1000;
    data &= 0b0111;
    if (data == face || data == 7) {
        if (is_extended && data != 7) {
            return PISTON_HEAD_TEXTURE_EXTENDED;
        }
        if (((PistonBase *) self)->data.sticky) {
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
    return (direction != 0 && level->getSignal(x, y - 1, z, 0))
        || (direction != 1 && level->getSignal(x, y + 1, z, 1))
        || (direction != 2 && level->getSignal(x, y, z - 1, 2))
        || (direction != 3 && level->getSignal(x, y, z + 1, 3))
        || (direction != 4 && level->getSignal(x - 1, y, z, 4))
        || (direction != 5 && level->getSignal(x + 1, y, z, 5))
        || level->getSignal(x, y, z, 0)
#ifdef P_USE_QC
        // Small tweak, pistons pointing up ignore QC on the block they are holding (such as a redstone block)
        || (direction != 1 && level->getSignal(x, y + 2, z, 1))
        || level->getSignal(x, y + 1, z - 1, 2)
        || level->getSignal(x, y + 1, z + 1, 3)
        || level->getSignal(x - 1, y + 1, z, 4)
        || level->getSignal(x + 1, y + 1, z, 5)
#endif
    ;
}

static bool replacable(int id) {
    return id == 0 || (8 <= id && id <= 11);
}

static bool pushable(Level *level, int x, int y, int z, int id) {
    if (replacable(id)) return true;
    Tile *t = Tile::tiles[id];
    if (!t) return false;
    if (id == BELT_ID) return true;
    if (id == 49 || id == 7 || t->destroyTime == -1 || id == 36 || id == 34) {
        return false;
    } else if (Tile::isEntityTile[id]) {
        return false;
    } else if (id == 29 || id == 33) {
        // Active pistons can't be pushed
        bool active = level->getData(x, y, z) & 0b1000;
        return !active;
    } else if (t->getRenderShape() != 0) {
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
        int id = level->getTile(xo, yo, zo);
        if (replacable(id)) break;
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
        int id = level->getTile(xo, yo, zo);
        if (replacable(id)) break;
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
        int id = level->getTile(bx, by, bz);
        int meta = level->getData(bx, by, bz);
        if (id == self->id && bx == x && by == y && bz == z) {
            bool sticky = ((PistonBase *) self)->data.sticky;
            level->setTileAndData(xo, yo, zo, 36, direction | (8 * sticky));
            TileEntity *te = (TileEntity *) make_MovingPistonTE(34, direction | (sticky * 8), direction, true);
            te->level = level;
            level->setTileEntity(xo, yo, zo, te);
        } else {
            level->setTileAndData(xo, yo, zo, 36, meta);
            TileEntity *te = (TileEntity *) make_MovingPistonTE(id, meta, direction, true);
            te->level = level;
            level->setTileEntity(xo, yo, zo, te);
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
        level->setData(x, y, z, direction | 8);
        return;
    }
    // Retract
    int ox = x + pis_dir[direction][0];
    int oy = y + pis_dir[direction][1];
    int oz = z + pis_dir[direction][2];
    TileEntity *te = level->getTileEntity(ox, oy, oz);
    if (te && te->vtable == get_piston_te_vtable()) {
        // Allow for retraction within a single tick
        MovingPistonTE_place((MovingPistonTE *) te);
    }
    // Retract the piston
    level->setTileAndData(x, y, z, 36, direction);
    te = (TileEntity *) make_MovingPistonTE(piston_head->id, direction | (8 * (self == sticky_piston_base)), direction, false);
    te->level = level;
    level->setTileEntity(x, y, z, te);
    // Retract the block
    if (((PistonBase *) self)->data.sticky) {
        ox += pis_dir[direction][0];
        oy += pis_dir[direction][1];
        oz += pis_dir[direction][2];
        int id = level->getTile(ox, oy, oz);
        if (pushable(level, ox, oy, oz, id) && id != 0) {
            int data = level->getData(ox, oy, oz);
            ox -= pis_dir[direction][0];
            oy -= pis_dir[direction][1];
            oz -= pis_dir[direction][2];
            level->setTileAndData(ox, oy, oz, 36, data);
            te = (TileEntity *) make_MovingPistonTE(id, data, direction, false);
            te->level = level;
            level->setTileEntity(ox, oy, oz, te);
            ox += pis_dir[direction][0];
            oy += pis_dir[direction][1];
            oz += pis_dir[direction][2];
            level->setTile(ox, oy, oz, 0);
        }
    }
}

void PistonBase_onRemove(UNUSED Tile *self, Level *level, int x, int y, int z) {
    int data = level->getData(x, y, z);
    int dir = data & 0b0111;
    if (dir == 0b111 || !(data & 0b1000)) return;
    int xo = x + pis_dir[dir][0];
    int yo = y + pis_dir[dir][1];
    int zo = z + pis_dir[dir][2];
    level->setData(xo, yo, zo, 0);
}

static void PistonBase_neighborChanged(Tile *self, Level *level, int x, int y, int z, UNUSED int neighborId) {
    int data = level->getData(x, y, z);
    int direction = data & 0b0111;
    if (direction == 7) {
        // Oh no... it's a silly pison! D:
        return;
    }
    bool is_extended = data & 0b1000;
    bool hasNeighborSignal = getNeighborSignal(level, x, y, z, direction);
    if (hasNeighborSignal && !is_extended) {
        if (canPushLine(level, x, y, z, direction)) {
            level->setTileAndDataNoUpdate(x, y, z, self->id, direction | 8);
            move(self, level, x, y, z, true, direction);
        }
    } else if (!hasNeighborSignal && is_extended) {
        level->setData(x, y, z, direction);
        move(self, level, x, y, z, false, direction);
    }
}

static int PistonBase_getRenderShape(UNUSED Tile *self) {
    return 50;
}

AABB *PistonBase_getAABB(Tile *self, UNUSED Level *level, int x, int y, int z) {
    //int data = level->getData(x, y, z);
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
    ((PistonBase *) piston)->data.sticky = sticky;
    ALLOC_CHECK(piston);
    int texture = 0;
    piston->constructor(id, texture, Material::wood);
    piston->texture = texture;

    // Set VTable
    piston->vtable = dup_vtable(Tile_vtable_base);
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
    piston->init();
    piston->setDestroyTime(0.5f);
    piston->setSoundType(Tile::SOUND_STONE);
    piston->category = 4;
    std::string name = !sticky ? "piston" : "pistonSticky";
    piston->setDescriptionId(name);

    if (sticky) {
        sticky_piston_base = piston;
    } else {
        piston_base = piston;
    }
}

// MovingPiston tileentity
static void MovingPistonTE_place(MovingPistonTE *self) {
    if (!self->super()->level) {
        return;
    }
    Level *level = self->super()->level;
    int x = self->super()->x, y = self->super()->y, z = self->super()->z, id = self->data.moving_id, meta = self->data.moving_meta;
    bool extending = self->data.extending;
    // Remove
    level->removeTileEntity(x, y, z);
    // Place
    if (level->getTile(x, y, z) == 36 && id) {
        if (id == piston_head->id && !extending) {
            if (meta & 0b1000) {
                id = sticky_piston_base->id;
            } else {
                id = piston_base->id;
            }
            meta &= 0b0111;
        }
        level->setTileAndData(
            x, y, z,
            id, meta
        );
        // Fix piston updating
        if (id == sticky_piston_base->id || id == piston_base->id)
        {
            // Update
            PistonBase_neighborChanged(Tile::tiles[id], level, x, y, z, 0);
        }
    }
}

static void MovingPistonTE_moveEntities(TileEntity *_self) {
    MovingPistonTE *self = (MovingPistonTE *) _self;
    // Do not do it if it is done
    if (self->data.pos >= 1.f) return;
    // Get AABB
    AABB aabb = {
        (float) self->super()->x,     (float) self->super()->y,        (float) self->super()->z,
        (float) self->super()->x + 1, (float) self->super()->y + 1.1f, (float) self->super()->z + 1
    };
    float pos = 1 - self->data.pos;
    if (self->data.extending) pos = -pos;
    int dir = self->data.direction;
    // There is a crash here for loading! `dir` is too big!
    aabb.x1 += pis_dir[dir][0] * pos;
    aabb.x2 += pis_dir[dir][0] * pos;
    aabb.y1 += pis_dir[dir][1] * pos;
    aabb.y2 += pis_dir[dir][1] * pos;
    aabb.z1 += pis_dir[dir][2] * pos;
    aabb.z2 += pis_dir[dir][2] * pos;

    // Get entities
    std::vector<Entity *> *entities = self->super()->level->getEntities(nullptr, aabb);
    float ox = pis_dir[dir][0] * P_SPEED;
    float oy = pis_dir[dir][1] * P_SPEED;
    float oz = pis_dir[dir][2] * P_SPEED;
    if (!self->data.extending) {
        ox = -ox; oy = -oy + 0.1; oz = -oz;
    }
    for (Entity *entity : *entities) {
        if (entity) {
            entity->move(ox, oy, oz);
        }
    }
}

static void MovingPistonTE_tick(TileEntity *_self) {
    // TODO: Fix piston clipping bug
    MovingPistonTE *self = (MovingPistonTE *) _self;
    int id = self->super()->level->getTile(self->super()->x, self->super()->y, self->super()->z);
    MovingPistonTE_moveEntities(_self);
    if (self->data.pos >= 1.f || id != 36) {
        MovingPistonTE_place(self);
        return;
    }
    self->data.pos = std::min(self->data.pos + P_SPEED, 1.f);
}

static bool MovingPistonTE_shouldSave(UNUSED TileEntity *self) {
    return true;
}

static void MovingPistonTE_load(TileEntity *self, CompoundTag *tag) {
    TileEntity_load->get(false)(self, tag);

    // TODO: Terrible!
    /*std::string str = "move_id";
    ((MovingPistonTE *) self)->moving_id = CompoundTag_getShort(tag, &str);
    str = "move_meta";
    ((MovingPistonTE *) self)->moving_meta = CompoundTag_getShort(tag, &str);
    str = "direction";
    ((MovingPistonTE *) self)->direction = CompoundTag_getShort(tag, &str);
    str = "extending";
    ((MovingPistonTE *) self)->extending = CompoundTag_getShort(tag, &str);*/
}

static bool MovingPistonTE_save(TileEntity *self, CompoundTag *tag) {
    TileEntity_save->get(false)(self, tag);

    // TODO: This just doesn't work!
    std::string str = "move_id";
    tag->putShort(str, ((MovingPistonTE *) self)->data.moving_id);
    str = "move_meta";
    tag->putShort(str, ((MovingPistonTE *) self)->data.moving_meta);
    str = "extending";
    tag->putShort(str, ((MovingPistonTE *) self)->data.extending);
    str = "direction";
    tag->putShort(str, ((MovingPistonTE *) self)->data.direction);
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
    piston_te->super()->constructor(49);
    piston_te->super()->renderer_id = 12;
    piston_te->super()->vtable = get_piston_te_vtable();
    piston_te->data.moving_id = moving_id;
    piston_te->data.moving_meta = moving_meta;
    piston_te->data.extending = extending;
    piston_te->data.direction = direction;
    return piston_te;
}

OVERWRITE_CALLS(TileEntityFactory_createTileEntity, TileEntity *, TileEntityFactory_createTileEntity_injection, (TileEntityFactory_createTileEntity_t original, int id)) {
    if (id == 49) {
        return (TileEntity *) make_MovingPistonTE();
    }
    // Call original
    return original(id);
}

OVERWRITE_CALLS(TileEntity_initTileEntities, void, TileEntity_initTileEntities_injection, (TileEntity_initTileEntities_t original)) {
    // Call original
    original();
    // Add
    std::string str = "Piston";
    TileEntity::setId(49, str);
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
    TileEntity *te = level->getTileEntity(x, y, z);
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
    TileEntity *te = level->getTileEntity(x, y, z);
    if (te != NULL) {
        te->level = level;
        MovingPistonTE_place((MovingPistonTE *) te);
    } else {
        level->setTileAndData(x, y, z, 0, 0);
    }
    return 1;
}

static AABB *MovingPiston_getAABB(EntityTile *self, UNUSED Level *level, int x, int y, int z) {
    TileEntity *te = level->getTileEntity(x, y, z);
    if (te != NULL && Tile::tiles[((MovingPistonTE *)te)->data.moving_id]) {
        Tile *t = Tile::tiles[((MovingPistonTE *)te)->data.moving_id];
        t->getAABB(level, x, y, z);
    }
    return Tile_getAABB->get(false)((Tile *) self, level, x, y, z);
}

static void MovingPiston_addAABBs(EntityTile *self, Level *level, int x, int y, int z, const AABB *intersecting, std::vector<AABB> &aabbs) {
    TileEntity *te = level->getTileEntity(x, y, z);
    if (te != NULL && Tile::tiles[((MovingPistonTE *)te)->data.moving_id]) {
        Tile *t = Tile::tiles[((MovingPistonTE *)te)->data.moving_id];
        t->addAABBs(level, x, y, z, intersecting, aabbs);
    }
    return Tile_addAABBs->get(false)((Tile *) self, level, x, y, z, intersecting, aabbs);
}

static void make_moving_piston(int id) {
    // TODO: wood sound -> stone
    piston_moving = EntityTile::allocate();
    ALLOC_CHECK(piston_moving);
    int texture = INVALID_TEXTURE;
    piston_moving->constructor(id, texture, Material::wood);

    // Set VTable
    piston_moving->vtable = dup_vtable(EntityTile_vtable_base);
    ALLOC_CHECK(piston_moving->vtable);
    piston_moving->vtable->newTileEntity = MovingPiston_newTileEntity;
    piston_moving->vtable->mayPlace2 = MovingPiston_mayPlace2;
    piston_moving->vtable->onRemove = MovingPiston_onRemove;
    piston_moving->vtable->getRenderShape = MovingPiston_getRenderShape;
    piston_moving->vtable->isSolidRender = MovingPiston_isSolidRender;
    piston_moving->vtable->getRenderLayer = MovingPiston_getRenderLayer;
    piston_moving->vtable->isCubeShaped = MovingPiston_isCubeShaped;
    piston_moving->vtable->getAABB = MovingPiston_getAABB;
    piston_moving->vtable->addAABBs = MovingPiston_addAABBs;
    piston_moving->vtable->use = MovingPiston_use;

    // Init
    piston_moving->init();
    piston_moving->setDestroyTime(-1.0f);
    piston_moving->category = 4;
    std::string name = "pistonMoving";
    piston_moving->setDescriptionId(name);
}

// Piston head
void PistonHead_onRemove(UNUSED Tile *self, Level *level, int x, int y, int z) {
    int dir = level->getData(x, y, z) & 0b0111;
    if (dir == 0b111) return;
    int xo = x - pis_dir[dir][0];
    int yo = y - pis_dir[dir][1];
    int zo = z - pis_dir[dir][2];
    int id = level->getTile(xo, yo, zo);
    int data = level->getData(xo, yo, zo);
    if ((id == sticky_piston_base->id || id == piston_base->id) && data == (dir | 8)) {
        level->setTile(xo, yo, zo, 0);
        // Add item
        ItemEntity *item_entity = (ItemEntity *) EntityFactory::CreateEntity(64, level);
        ALLOC_CHECK(item_entity);
        ItemInstance item = {.count = 1, .id = id, .auxiliary = 0};
        item_entity->constructor(level, xo + 0.5f, yo, zo + 0.5f, item);
        item_entity->moveTo(xo + 0.5f, yo, zo + 0.5f, 0, 0);
        level->addEntity((Entity *) item_entity);
    }
}

static void PistonHead_neighborChanged(UNUSED Tile *self, Level *level, int x, int y, int z, UNUSED int neighborId) {
    int dir = level->getData(x, y, z) & 0b0111;
    if (dir == 0b111) return;
    int xo = x - pis_dir[dir][0];
    int yo = y - pis_dir[dir][1];
    int zo = z - pis_dir[dir][2];
    int id = level->getTile(xo, yo, zo);
    int data = level->getData(xo, yo, zo);
    if ((id != sticky_piston_base->id && id != piston_base->id) || data != (dir | 8)) {
        level->setTile(x, y, z, 0);
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
            self->setShape(0, 0, 0, 1.f, AABB_HI, 1.f);
            callback();
            self->setShape(AABB_BI, AABB_HI, AABB_BI, AABB_BO, 1.f, AABB_BO);
            callback();
            break;
        case 0b001:
            self->setShape(0, AABB_HO, 0, 1.f, 1.f, 1.f);
            callback();
            self->setShape(AABB_BI, 0, AABB_BI, AABB_BO, AABB_HO, AABB_BO);
            callback();
            break;
        case 0b010:
            self->setShape(0, 0, 0, 1.f, 1.f, AABB_HI);
            callback();
            self->setShape(AABB_BI, AABB_BI, AABB_HI, AABB_BO, AABB_BO, 1);
            callback();
            break;
        case 0b011:
            self->setShape(0, 0, AABB_HO, 1.f, 1.f, 1.f);
            callback();
            self->setShape(AABB_BI, AABB_BI, 0, AABB_BO, AABB_BO, AABB_HO);
            callback();
            break;
        case 0b100:
            self->setShape(0, 0, 0, AABB_HI, 1.f, 1.f);
            callback();
            self->setShape(AABB_HI, AABB_BI, AABB_BI, 1, AABB_BO, AABB_BO);
            callback();
            break;
        case 0b101:
            self->setShape(AABB_HO, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
            callback();
            self->setShape(0, AABB_BI, AABB_BI, AABB_HO, AABB_BO, AABB_BO);
            callback();
            break;
        default:
            break;
    }
    self->setShape(0, 0, 0, 1, 1, 1);
}
static void PistonHead_addAABBs(Tile *self, Level *level, int x, int y, int z, const AABB *intersecting, std::vector<AABB> &aabbs) {
    int data = level->getData(x, y, z);
    PistonHead_setShapesCallback(
        self, data, [&]{
            Tile_addAABBs->get(false)(self, level, x, y, z, intersecting, aabbs);
        }
    );
}

OVERWRITE_CALLS(TileRenderer_tesselateInWorld, bool, TileRenderer_tesselateInWorld_injection, (TileRenderer_tesselateInWorld_t original, TileRenderer *self, Tile *tile, int x, int y, int z)) {
    if (tile == piston_head) {
        int data = self->level->getData(x, y, z);
        PistonHead_setShapesCallback(
            tile, data, [&]{
                original(self, tile, x, y, z);
            }
        );
        return 1;
    } else {
        return original(self, tile, x, y, z);
    }
}

OVERWRITE_CALLS(TileRenderer_renderTile, void, TileRenderer_renderTile_injection, (TileRenderer_renderTile_t original, TileRenderer *self, Tile *tile, int aux)) {
    if (tile == piston_head) {
        PistonHead_setShapesCallback(
            tile, aux, [&]{
                original(self, tile, aux);
            }
        );
    } else {
        original(self, tile, aux);
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
    piston_head = Tile::allocate();
    ALLOC_CHECK(piston_head);
    // TODO: Texture
    int texture = 6*16+11;
    piston_head->constructor(id, texture, Material::wood);
    piston_head->texture = texture;

    // Set VTable
    piston_head->vtable = dup_vtable(Tile_vtable_base);
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
    piston_head->init();
    piston_head->setDestroyTime(0.5f);
    piston_head->setSoundType(Tile::SOUND_STONE);
    piston_head->category = 4;
    std::string name = "piston_head";
    piston_head->setDescriptionId(name);
}

// Rendering moving piston
static void PistonTileEntityRenderer_render(UNUSED TileEntityRenderer *self, TileEntity *tileentity, float x, float y, float z, float unknown) {
    MovingPistonTE *piston_te = (MovingPistonTE *) tileentity;
    Tile *t = Tile::tiles[piston_te->data.moving_id];
    if (!t) return;
    float pos = (piston_te->data.pos - P_SPEED) + (P_SPEED * std::min(unknown, 1.f));
    float xo = (1 - pos) * pis_dir[piston_te->data.direction][0];
    float yo = (1 - pos) * pis_dir[piston_te->data.direction][1];
    float zo = (1 - pos) * pis_dir[piston_te->data.direction][2];
    if (!piston_te->data.extending) {
        xo = -xo;
        yo = -yo;
        zo = -zo;
    }

    glPushMatrix();
    glTranslatef(x + 0.5 - xo, y + 0.5 - yo, z + 0.5 - zo);

    std::string terrain = "terrain.png";
    EntityRenderer_bindTexture->get(false)(NULL, terrain);
    //glDisable(GL_LIGHTING);

    TileRenderer *tr = ItemRenderer::tileRenderer;
    tr->level = (LevelSource *) piston_te->super()->level;
    tr->renderTile(t, piston_te->data.moving_meta);

    if (t->id == piston_head->id && !piston_te->data.extending) {
        // Render piston base too
        glTranslatef(xo, yo, zo);
        Tile *t = 0;
        if (piston_te->data.moving_meta & 0b1000) {
            t = sticky_piston_base;
        } else {
            t = piston_base;
        }
        tr->renderTile(t, piston_te->data.moving_meta & 0b0111);
    }

    glPopMatrix();
}

CUSTOM_VTABLE(piston_ter, TileEntityRenderer) {
    vtable->render = PistonTileEntityRenderer_render;
}

static TileEntityRenderer *make_piston_tile_entity_renderer() {
    TileEntityRenderer *piston_ter = TileEntityRenderer::allocate();
    ALLOC_CHECK(piston_ter);
    piston_ter->vtable = get_piston_ter_vtable();
    return piston_ter;
}

OVERWRITE_CALLS(TileEntityRenderDispatcher_constructor, TileEntityRenderDispatcher *, TileEntityRenderDispatcher_constructor_injection, (TileEntityRenderDispatcher_constructor_t original, TileEntityRenderDispatcher *self)) {
    // Call original
    original(self);
    // Add pedestal renderer
    TileEntityRenderer *pistonTileEntityRenderer = make_piston_tile_entity_renderer();
    self->renderer_map.insert(std::make_pair(12, pistonTileEntityRenderer));
    return self;
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
