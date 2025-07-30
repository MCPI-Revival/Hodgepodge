#include <functional>
#include <cmath>

#include <GLES/gl.h>

//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>

#include "rendering.h"
#include "piston.h"
#include "api.h"
#include "init.h"
#include "belt.h"

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
static constexpr int pis_dir[6][3] = {
    {0, -1, 0}, {0, 1, 0},
    {0, 0, -1}, {0, 0, 1},
    {-1, 0, 0}, {1, 0, 0}
};
Tile *piston_base = NULL;
Tile *sticky_piston_base = NULL;
EntityTile *piston_moving = NULL;
Tile *piston_head = NULL;

struct MovingPistonTE final : CustomTileEntity {
    int moving_id;
    int moving_meta;
    int direction;
    bool extending;
    float pos = 0;

    MovingPistonTE(
        int moving_id,
        int moving_meta,
        int direction,
        bool extending
    ) : CustomTileEntity(49), moving_id(moving_id), moving_meta(moving_meta), direction(direction), extending(extending) {
        this->self->renderer_id = 12;
    }

    void place() {
        if (!self->level) {
            return;
        }
        int x = self->x, y = self->y, z = self->z, id = moving_id, meta = moving_meta;
        // Remove
        TileEntity *te = self->level->getTileEntity(x, y, z);
        if (te == NULL) {
            WARN("The level->getTileEntity(%i, %i, %i) is %i! This is a bug! A moving block has been deleted! Please report this to Hodgepodge!", x, y, z, te);
        }
        // Do not change this, `self` can be deleted in `removeTileEntity`
        Level *level = self->level;
        self->pending_removal = true;
        level->removeTileEntity(x, y, z);
        // Place
        if (level->getTile(x, y, z) == 36 && id) {
            if (id == piston_head->id && !this->extending) {
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
                Tile::tiles[id]->neighborChanged(level, x, y, z, 0);
            }
        }
    }

    void moveEntities() {
        // Do not do it if it is done
        if (pos >= 1.f) return;
        // Get AABB
        AABB aabb = {
            (float) self->x,     (float) self->y,        (float) self->z,
            (float) self->x + 1, (float) self->y + 1.1f, (float) self->z + 1
        };
        float pos = 1 - pos;
        if (this->extending) pos = -pos;
        int dir = direction;
        // There is a crash here for loading! `dir` is too big!
        aabb.x1 += pis_dir[dir][0] * pos;
        aabb.x2 += pis_dir[dir][0] * pos;
        aabb.y1 += pis_dir[dir][1] * pos;
        aabb.y2 += pis_dir[dir][1] * pos;
        aabb.z1 += pis_dir[dir][2] * pos;
        aabb.z2 += pis_dir[dir][2] * pos;

        // Get entities
        std::vector<Entity *> *entities = self->level->getEntities(nullptr, aabb);
        float ox = pis_dir[dir][0] * P_SPEED;
        float oy = pis_dir[dir][1] * P_SPEED;
        float oz = pis_dir[dir][2] * P_SPEED;
        if (!this->extending) {
            ox = -ox; oy = -oy + 0.1; oz = -oz;
        }
        for (Entity *entity : *entities) {
            if (entity) {
                entity->move(ox, oy, oz);
            }
        }
    }

    void tick() override {
        // TODO: Fix piston clipping bug
        int id = self->level->getTile(self->x, self->y, self->z);
        moveEntities();
        if (pos >= 1.f || id != 36) {
            place();
            return;
        }
        pos = std::min(pos + P_SPEED, 1.f);
    }

    bool shouldSave() override {
        return true;
    }

    void load(CompoundTag *tag) override {
        TileEntity_load->get(false)(self, tag);

        // TODO: Terrible!
        /*std::string str = "move_id";
        moving_id = CompoundTag_getShort(tag, &str);
        str = "move_meta";
        moving_meta = CompoundTag_getShort(tag, &str);
        str = "direction";
        direction = CompoundTag_getShort(tag, &str);
        str = "extending";
        extending = CompoundTag_getShort(tag, &str);*/
    }

    bool save(CompoundTag *tag) override {
        TileEntity_save->get(false)(self, tag);

        // TODO: This just doesn't work!
        /*std::string str = "move_id";
        tag->putShort(str, moving_id);
        str = "move_meta";
        tag->putShort(str, moving_meta);
        str = "extending";
        tag->putShort(str, extending);
        str = "direction";
        tag->putShort(str, direction);*/
        return true;
    }
};

// MovingPiston tileentity
OVERWRITE_CALLS(TileEntityFactory_createTileEntity, TileEntity *, TileEntityFactory_createTileEntity_injection, (TileEntityFactory_createTileEntity_t original, int id)) {
    if (id == 49) {
        return (TileEntity *) (new MovingPistonTE(30, 0, 0, false))->self;
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

// Piston Tile
struct PistonBase final : CustomTile {
    bool sticky;

    PistonBase(const int id, const int texture, const Material *material, bool sticky): CustomTile(id, texture, material), sticky(sticky) {}

    bool isSolidRender() override {
        return false;
    }

    void setPlacedBy(Level *level, int x, int y, int z, Mob *placer) override {
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
        neighborChanged(level, x, y, z, 0);
    }

    int getTexture3(UNUSED LevelSource *levelsrc, int x, int y, int z, int face) override {
        int data = mc->level->getData(x, y, z);
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

    // Piston logic functions
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
        return
            // Air
            id == 0
            // Liquids
            || (8 <= id && id <= 11)
        ;
    }
    static bool pushable(Level *level, int x, int y, int z, int id) {
        if (replacable(id)) return true;
        Tile *t = Tile::tiles[id];
        if (!t) return false;
        if (id == BELT_ID) return true;
        if (id == 49 /* Obsidian */ || id == 34 /* Piston Head */ || t->destroyTime == -1 /* Unbreakable */) {
            // Hardcoded blocks
            return false;
        } else if (Tile::isEntityTile[id]) {
            // Tile Entities
            return false;
        } else if (id == 29 || id == 33) {
            // Active pistons can't be pushed
            bool active = level->getData(x, y, z) & 0b1000;
            return !active;
        } else if (t->getRenderShape() != 0) {
            // Temp thingy (bc I'm too lazy to find out what weirdly shaped stuff can move)
            // TODO: Remove temp thingy (bc I'm too lazy to find out what weirdly shaped stuff can move)
            return false;
        }
        return true;
    }
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

    // Piston movement functions
    bool extend(Level *level, int x, int y, int z, int direction) {
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
                level->setTileAndData(xo, yo, zo, 36, direction | (8 * sticky));
                TileEntity *te = (new MovingPistonTE(34, direction | (sticky * 8), direction, true))->self;
                te->level = level;
                level->setTileEntity(xo, yo, zo, te);
            } else {
                level->setTileAndData(xo, yo, zo, 36, meta);
                TileEntity *te = (new  MovingPistonTE(id, meta, direction, true))->self;
                te->level = level;
                level->setTileEntity(xo, yo, zo, te);
            }
            xo = bx;
            yo = by;
            zo = bz;
        }
        return true;
    }
    void move(Level *level, int x, int y, int z, bool extending, int data) {
        int direction = data & 0b0111;
        if (extending) {
            if (!extend(level, x, y, z, direction)) return;
            level->setData(x, y, z, direction | 8);
            return;
        }
        // Retract
        int ox = x + pis_dir[direction][0];
        int oy = y + pis_dir[direction][1];
        int oz = z + pis_dir[direction][2];
        TileEntity *te = level->getTileEntity(ox, oy, oz);
        if (te && te->id == 49 /* MovingPistonTE */) {
            // Allow for retraction within a single tick
            ((MovingPistonTE *) custom_get<CustomTileEntity>(te))->place();
        }
        // Retract the piston
        level->setTileAndData(x, y, z, 36, direction);
        te = (new MovingPistonTE(piston_head->id, direction | (8 * (self->id == sticky_piston_base->id)), direction, false))->self;
        te->level = level;
        level->setTileEntity(x, y, z, te);
        // Retract the block
        if (sticky) {
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
                te = (new MovingPistonTE(id, data, direction, false))->self;
                te->level = level;
                level->setTileEntity(ox, oy, oz, te);
                ox += pis_dir[direction][0];
                oy += pis_dir[direction][1];
                oz += pis_dir[direction][2];
                level->setTile(ox, oy, oz, 0);
            } else {
                ox -= pis_dir[direction][0];
                oy -= pis_dir[direction][1];
                oz -= pis_dir[direction][2];
                level->setTile(ox, oy, oz, 0);
            }
        } else {
            level->setTile(ox, oy, oz, 0);
        }
    }

    void onRemove(Level *level, int x, int y, int z) override {
        int data = level->getData(x, y, z);
        int dir = (data & 0b0111);
        if (dir == 0b111 || !(data & 0b1000)) return;
        // Remove the head
        int xo = x + pis_dir[dir][0];
        int yo = y + pis_dir[dir][1];
        int zo = z + pis_dir[dir][2];
        level->setTile(xo, yo, zo, 0);
    }

    void neighborChanged(Level *level, int x, int y, int z, UNUSED int neighborId) override {
        int data = level->getData(x, y, z);
        int direction = data & 0b0111;
        if (direction == 7) {
            // "Silly Piston" aka invalid meta piston base
            return;
        }
        bool is_extended = data & 0b1000;
        bool hasNeighborSignal = getNeighborSignal(level, x, y, z, direction);
        if (hasNeighborSignal && !is_extended) {
            if (canPushLine(level, x, y, z, direction)) {
                level->setTileAndDataNoUpdate(x, y, z, self->id, direction | 8);
                move(level, x, y, z, true, direction);
            }
        } else if (!hasNeighborSignal && is_extended) {
            level->setData(x, y, z, direction);
            move(level, x, y, z, false, direction);
        }
    }

    int getRenderShape() override {
        return 50;
    }

    AABB *getAABB(UNUSED Level *level, int x, int y, int z) override {
        int data = level->getData(x, y, z);
        int dir = data & 0b0111, on = data & 0b1000;
        self->aabb.x1 = x;
        self->aabb.y1 = y;
        self->aabb.z1 = z;
        self->aabb.x2 = x + 1.f;
        self->aabb.y2 = y + 1.f;
        self->aabb.z2 = z + 1.f;
        if (on && data != 0b1111) {
            if (dir == 0) {
                self->aabb.y1 += 0.25;
            } else if (dir == 1) {
                self->aabb.y2 -= 0.25;
            } else if (dir == 2) {
                self->aabb.z1 += 0.25;
            } else if (dir == 3) {
                self->aabb.z2 -= 0.25;
            } else if (dir == 4) {
                self->aabb.x1 += 0.25;
            } else if (dir == 5) {
                self->aabb.x2 -= 0.25;
            }
        }
        // Set shape, it's wrong but why not?
        /*self->x1 = self->aabb.x1;
        self->y1 = self->aabb.y1;
        self->z1 = self->aabb.z1;
        self->x2 = self->aabb.x2;
        self->y2 = self->aabb.y2;
        self->z2 = self->aabb.z2;*/
        return &self->aabb;
    }

    bool isCubeShaped() override {
        return false;
    }

    void updateShape(LevelSource *level, int x, int y, int z) override {
        int data = level->getData(x, y, z);
        int dir = data & 0b0111, on = data & 0b1000;
        AABB aabb{0, 0, 0, 1, 1, 1};
        if (on && data != 0b1111) {
            if (dir == 0) {
                aabb.y1 += 0.25;
            } else if (dir == 1) {
                aabb.y2 -= 0.25;
            } else if (dir == 2) {
                aabb.z1 += 0.25;
            } else if (dir == 3) {
                aabb.z2 -= 0.25;
            } else if (dir == 4) {
                aabb.x1 += 0.25;
            } else if (dir == 5) {
                aabb.x2 -= 0.25;
            }
        }
        setShape(aabb.x1, aabb.y1, aabb.z1, aabb.x2, aabb.y2, aabb.z2);
    }
};

static void make_piston(int id, bool sticky) {
    // Construct
    Tile *piston = (new PistonBase(id, 0, Material::wood, sticky))->self;

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

// MovingPiston entitytile
struct MovingPiston final : CustomEntityTile {
    MovingPiston(const int id, const int texture, const Material *material) : CustomEntityTile(id, texture, material) {}

    TileEntity *newTileEntity() override {
        // Don't implicitly make the piston tile entity
        // If it wasn't creating properly, don't force it
        return NULL;
    }

    bool mayPlace2(UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) override {
        // mayPlace doesn't need to be overrode because Tile_mayPlace calls mayPlace2
        return false;
    }

    void onRemove(Level *level, int x, int y, int z) override {
        TileEntity *te = level->getTileEntity(x, y, z);
        if (te != NULL) {
            ((MovingPistonTE *) custom_get<CustomTileEntity>(te))->place();
        }
    }

    int getRenderShape() override {
        return -1;
    }

    bool isSolidRender() override {
        return false;
    }

    int getRenderLayer() override {
        return 1;
    }

    bool isCubeShaped() override {
        return false;
    }

    bool use(Level *level, int x, int y, int z, UNUSED Player *player) override {
        TileEntity *te = level->getTileEntity(x, y, z);
        if (te != NULL) {
            te->level = level;
            ((MovingPistonTE *) custom_get<CustomTileEntity>(te))->place();
        } else {
            level->setTileAndData(x, y, z, 0, 0);
        }
        return true;
    }

    AABB *getAABB(UNUSED Level *level, int x, int y, int z) override {
        TileEntity *te = level->getTileEntity(x, y, z);
        if (te != NULL && Tile::tiles[((MovingPistonTE *) custom_get<CustomTileEntity>(te))->moving_id]) {
            Tile *t = Tile::tiles[((MovingPistonTE *) custom_get<CustomTileEntity>(te))->moving_id];
            t->getAABB(level, x, y, z);
        }
        return Tile_getAABB->get(false)((Tile *) self, level, x, y, z);
    }

    void addAABBs(Level *level, int x, int y, int z, const AABB *intersecting, std::vector<AABB> &aabbs) override {
        TileEntity *te = level->getTileEntity(x, y, z);
        if (te != NULL && Tile::tiles[((MovingPistonTE *) custom_get<CustomTileEntity>(te))->moving_id]) {
            Tile *t = Tile::tiles[((MovingPistonTE *) custom_get<CustomTileEntity>(te))->moving_id];
            t->addAABBs(level, x, y, z, intersecting, aabbs);
        }
        return Tile_addAABBs->get(false)((Tile *) self, level, x, y, z, intersecting, aabbs);
    }
};

static void make_moving_piston(int id) {
    // TODO: wood sound -> stone
    piston_moving = (new MovingPiston(id, INVALID_TEXTURE, Material::wood))->self;

    // Init
    piston_moving->init();
    piston_moving->setDestroyTime(-1.0f);
    piston_moving->category = 4;
    std::string name = "pistonMoving";
    piston_moving->setDescriptionId(name);
}

// Piston head
struct PistonHead final : CustomTile {
    PistonHead(const int id, const int texture, const Material *material) : CustomTile(id, texture, material) {}

    void onRemove(Level *level, int x, int y, int z) {
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
            ItemInstance item = {.count = 1, .id = id, .auxiliary = 0};
            item_entity->constructor(level, xo + 0.5f, yo, zo + 0.5f, item);
            item_entity->moveTo(xo + 0.5f, yo, zo + 0.5f, 0, 0);
            level->addEntity((Entity *) item_entity);
        }
    }

    void neighborChanged(Level *level, int x, int y, int z, UNUSED int neighborId) override {
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

    void onPlace(Level *level, int x, int y, int z) override {
        // Check
        neighborChanged(level, x, y, z, -1);
    }

    int getTexture2(UNUSED int face, int data) override {
        return (data & 0b1000) ?
            // Sticky
            6*16+10:
            // Not sticky
            6*16+11;
    }

    static constexpr float AABB_BI = 0.375f;
    static constexpr float AABB_BO = (1 - AABB_BI);
    static constexpr float AABB_HI = 0.25f;
    static constexpr float AABB_HO = (1 - AABB_HI);
    static void setShapesCallback(Tile *self, int data, const std::function<void()> callback) {
        int dir = data & 0b0111;
        switch (dir) {
            case 0b000:
                self->setShape(0, 0, 0, 1.f, AABB_HI, 1.f);
                callback();
                self->setShape(AABB_BI, AABB_HI, AABB_BI, AABB_BO, 1.25f, AABB_BO);
                callback();
                break;
            case 0b001:
                self->setShape(0, AABB_HO, 0, 1.f, 1.f, 1.f);
                callback();
                self->setShape(AABB_BI, -0.25f, AABB_BI, AABB_BO, AABB_HO, AABB_BO);
                callback();
                break;
            case 0b010:
                self->setShape(0, 0, 0, 1.f, 1.f, AABB_HI);
                callback();
                self->setShape(AABB_BI, AABB_BI, AABB_HI, AABB_BO, AABB_BO, 1.25f);
                callback();
                break;
            case 0b011:
                self->setShape(0, 0, AABB_HO, 1.f, 1.f, 1.f);
                callback();
                self->setShape(AABB_BI, AABB_BI, -0.25f, AABB_BO, AABB_BO, AABB_HO);
                callback();
                break;
            case 0b100:
                self->setShape(0, 0, 0, AABB_HI, 1.f, 1.f);
                callback();
                self->setShape(AABB_HI, AABB_BI, AABB_BI, 1.25f, AABB_BO, AABB_BO);
                callback();
                break;
            case 0b101:
                self->setShape(AABB_HO, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
                callback();
                self->setShape(-0.25f, AABB_BI, AABB_BI, AABB_HO, AABB_BO, AABB_BO);
                callback();
                break;
            default:
                break;
        }
        // TODO: Handle "Silly Piston" head
        self->setShape(0, 0, 0, 1, 1, 1);
    }
    void addAABBs(Level *level, int x, int y, int z, const AABB *intersecting, std::vector<AABB> &aabbs) override {
        int data = level->getData(x, y, z);
        setShapesCallback(
            (Tile *) self, data, [&]{
                Tile_addAABBs->get(false)((Tile *) self, level, x, y, z, intersecting, aabbs);
            }
        );
    }

    int getResource(UNUSED int data, UNUSED Random *random) override {
        return 0;
    }

    bool isCubeShaped() override {
        return false;
    }

    bool isSolidRender() override {
        return false;
    }
};

OVERWRITE_CALLS(TileRenderer_tesselateInWorld, bool, TileRenderer_tesselateInWorld_injection, (TileRenderer_tesselateInWorld_t original, TileRenderer *self, Tile *tile, int x, int y, int z)) {
    if (tile == piston_head) {
        int data = self->level->getData(x, y, z);
        PistonHead::setShapesCallback(
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
        PistonHead::setShapesCallback(
            tile, aux, [&]{
                original(self, tile, aux);
            }
        );
    } else {
        original(self, tile, aux);
    }
}

static void make_piston_head(int id) {
    // Construct
    // TODO: Fix texture
    piston_head = (Tile *) (new PistonHead(id, 6*16+11, Material::wood))->self;

    // Init
    piston_head->init();
    piston_head->setDestroyTime(0.5f);
    piston_head->setSoundType(Tile::SOUND_STONE);
    piston_head->category = 4;
    std::string name = "piston_head";
    piston_head->setDescriptionId(name);
}

// Rendering moving piston
struct PistonTileEntityRenderer final : CustomTileEntityRenderer {
    PistonTileEntityRenderer() : CustomTileEntityRenderer() {}

    void render(TileEntity *tileentity, float x, float y, float z, float unknown) override {
        MovingPistonTE *piston_te = (MovingPistonTE *) custom_get<CustomTileEntity>(tileentity);
        Tile *t = Tile::tiles[piston_te->moving_id];
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

        media_glPushMatrix();
        media_glTranslatef(x + 0.5 - xo, y + 0.5 - yo, z + 0.5 - zo);

        std::string terrain = "terrain.png";
        EntityRenderer_bindTexture->get(false)(NULL, terrain);
        //glDisable(GL_LIGHTING);

        TileRenderer *tr = ItemRenderer::tileRenderer;
        tr->level = (LevelSource *) piston_te->self->level;
        tr->renderTile(t, piston_te->moving_meta);
        media_glPopMatrix();

        if (t->id == piston_head->id && !piston_te->extending) {
            // Render piston base too
            media_glPushMatrix();
            media_glTranslatef(x + 0.5, y + 0.5, z + 0.5);
            EntityRenderer_bindTexture->get(false)(NULL, terrain);
            if (piston_te->moving_meta & 0b1000) {
                t = sticky_piston_base;
            } else {
                t = piston_base;
            }

            // TODO: FIX THIS ASAP
            //TileRenderer_tesselatePiston(t, 0, 0, 0, piston_te->moving_meta & 0b0111, piston_te->self->level);
            // Ugly hack, a cobblestone looks kind of like a piston base, assuming you've never seen either
            tr->renderTile(Tile::tiles[4], 0);

            media_glPopMatrix();
        }
    }
};

OVERWRITE_CALLS(TileEntityRenderDispatcher_constructor, TileEntityRenderDispatcher *, TileEntityRenderDispatcher_constructor_injection, (TileEntityRenderDispatcher_constructor_t original, TileEntityRenderDispatcher *self)) {
    // Call original
    original(self);
    // Add pedestal renderer
    TileEntityRenderer *pistonTileEntityRenderer = (new PistonTileEntityRenderer())->self;
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
