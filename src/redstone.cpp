#include <cmath>

//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "redstone.h"

// Items/tiles
static Item *redstone = NULL;
static Tile *redstone_wire = NULL;
static Item *repeater_item = NULL;
static Tile *inactive_repeater = NULL;
static Tile *active_repeater = NULL;
static Tile *redstone_block = NULL;
static Tile *active_redstone_block = NULL;
static Tile *redstone_torch = NULL;
static Tile *lamp = NULL;
static Tile *active_lamp = NULL;

int RedStoneOreTile_getResource_injection(UNUSED Tile *t, UNUSED int data, UNUSED Random *random) {
    return REDSTONE_ID;
}

static bool canBePlacedOn(Level *level, int x, int y, int z) {
    int id = level->getTile(x, y, z);
    // Glass
    return id == 20
        // Double slab
        || id == 43
        // Slab
        || (id == 44 && level->getData(x, y, z) > 6)
        || level->isSolidRenderTile(x, y, z);
}

static void make_redstone_tileitems() {
    // Redstone dust
    // TilePlanterItem's constructor was inlined
    redstone = (Item *) TilePlanterItem::allocate();
    redstone->constructor(REDSTONE_ID - 256);
    redstone->vtable = (Item::VTable *) TilePlanterItem::VTable::base;
    ((TilePlanterItem *) redstone)->tile_id = redstone_wire->id;
    std::string name = "redstoneDust";
    redstone->setDescriptionId(name);
    redstone->category = 2;
    redstone->texture = 56;

    // Repeater
    repeater_item = (Item *) TilePlanterItem::allocate();
    repeater_item->constructor(REPEATER_ID - 256);
    repeater_item->vtable = (Item::VTable *) TilePlanterItem::VTable::base;
    ((TilePlanterItem *) repeater_item)->tile_id = inactive_repeater->id;
    name = "repeater";
    repeater_item->setDescriptionId(name);
    repeater_item->category = 2;
    repeater_item->texture = 86;
}

// Redstone tile
static std::vector<Vec3> wire_neighbors = {};
static bool checking_power = false;
static void updateWires(Level *level, int x, int y, int z) {
    if (level->getTile(x, y, z) != 55) return;
    level->updateNearbyTiles(x,     y,     z,     55);
    level->updateNearbyTiles(x - 1, y,     z,     55);
    level->updateNearbyTiles(x + 1, y,     z,     55);
    level->updateNearbyTiles(x,     y - 1, z,     55);
    level->updateNearbyTiles(x,     y + 1, z,     55);
    level->updateNearbyTiles(x,     y,     z - 1, 55);
    level->updateNearbyTiles(x,     y,     z + 1, 55);
}

bool canWireConnectTo(LevelSource *level, int x, int y, int z, int side) {
    int id = level->getTile(x, y, z);
    if (id == 55) {
        return true;
    } else if (id == 0) {
        return false;
    } else if (Tile::tiles[id]->isSignalSource()) {
        return true;
    } else if (id == inactive_repeater->id || id == active_repeater->id) {
        // Yessir, another special case!
        int dir = level->getData(x, y, z) & 0b0011;
        return (dir ^ 2) == side;
    }
    return false;
}

struct RedstoneWire final : CustomTile {
    RedstoneWire(const int id, const int texture, const Material *material): CustomTile(id, texture, material) {}

    bool isSignalSource() override {
        return true;
    }

    AABB *getAABB(UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) override {
        return NULL;
    }
    bool isSolidRender() override {
        return false;
    }
    bool isCubeShaped() override {
        return false;
    }
    int getRenderShape() override {
        return 52;
    }
    bool mayPlace2(Level *level, int x, int y, int z) override {
        return canBePlacedOn(level, x, y - 1, z);
    }

    int getRenderLayer() override {
        return 1;
    }

    int getResourceCount(UNUSED Random *random) override {
        return 1;
    }

    int getColor(LevelSource *level_source, int x, int y, int z) override {
        int data = level_source->getData(x, y, z) * 7;
        if (data == 0) return 0x660000;
        return 0x880000 + (data << 16);
    }

    static int maxToCur(Level *level, int x, int y, int z, int other) {
        if (level->getTile(x, y, z) != 55) {
            return other;
        } else {
            int data = level->getData(x, y, z);
            return data > other ? data : other;
        }
    }

    static void update(Level *level, int x, int y, int z, int x2, int y2, int z2) {
        int data = level->getData(x, y, z);
        int power = 0;
        checking_power = true;
        bool powered = level->hasNeighborSignal(x, y, z);
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
                    power = maxToCur(level, xo, y, zo, power);
                }
                if (
                    level->isSolidRenderTile(xo, y, zo)
                    && !level->isSolidRenderTile(x, y + 1, z)
                ) {
                    // Diagonal, +y
                    if (xo != x2 || y + 1 != y2 || zo != z2) {
                        power = maxToCur(level, xo, y + 1, zo, power);
                    }
                } else if (
                    !level->isSolidRenderTile(xo, y, zo)
                    && (xo != x2 || y - 1 != y2 || zo != z2)
                ) {
                    // Diagonal, -y
                    power = maxToCur(level, xo, y - 1, zo, power);
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
            level->setData(x, y, z, power);
            //Level_setTileDirty(level, x, y, z);
            //level->no_update = false;

            for (int side = 0; side < 4; side++) {
                // Check each side
                int xo = x, yo = y - 1, zo = z;
                if (side == 0) xo--;
                else if (side == 1) xo++;
                else if (side == 2) zo--;
                else if (side == 3) zo++;

                if (level->isSolidRenderTile(xo, y, zo)) {
                    yo += 2;
                }
                int cur = maxToCur(level, xo, y, zo, -1);
                power = level->getData(x, y, z);
                if (power > 0) power--;
                if (cur >= 0 && cur != power) {
                    update(level, xo, y, zo, x, y, z);
                }

                cur = maxToCur(level, xo, yo, zo, -1);
                power = level->getData(x, y, z);
                if (power > 0) power--;
                if (cur >= 0 && cur != power) {
                    update(level, xo, yo, zo, x, y, z);
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

    static void propagate(Level *level, int x, int y, int z) {
        update(level, x, y, z, x, y, z);
        std::vector<Vec3> neighbors = wire_neighbors;
        wire_neighbors = {};
        for (const Vec3 &i : neighbors) {
            level->updateNearbyTiles(i.x, i.y, i.z, 55);
        }
    }

    void neighborChanged(Level *level, int x, int y, int z, UNUSED int neighborId) override {
        if (!mayPlace2(level, x, y, z)) {
            level->setTile(x, y, z, 0);
            // TODO drop on ground
            return;
        } else {
            propagate(level, x, y, z);
        }
    }

    #define UPDATE_IF_SOLID(x2, y2, z2) \
        if (level->isSolidRenderTile((x2), (y2), (z2))) { \
            updateWires(level, (x2), (y2) + 1, (z2)); \
        } else { \
            updateWires(level, (x2), (y2) - 1, (z2)); \
        }
    void onPlaceOrRemove(Level *level, int x, int y, int z) {
        level->updateNearbyTiles(x, y + 1, z, 55);
        level->updateNearbyTiles(x, y - 1, z, 55);
        propagate(level, x, y, z);
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
    void onPlace(Level *level, int x, int y, int z) override {
        onPlaceOrRemove(level, x, y, z);
    }
    void onRemove(Level *level, int x, int y, int z) override {
        onPlaceOrRemove(level, x, y, z);
    }

    bool getSignal2(LevelSource *level, int x, int y, int z, int side) override {
        if (checking_power) return false;
        if (level->getData(x, y, z) == 0) return false;

        // The block under it is always powered
        if (side == 1) return true;

        bool on_xm = (canWireConnectTo(level, x - 1, y, z, 1) || (!level->isSolidRenderTile(x - 1, y, z) && canWireConnectTo(level, x - 1, y - 1, z, -1)));
        bool on_xp = (canWireConnectTo(level, x + 1, y, z, 3) || (!level->isSolidRenderTile(x + 1, y, z) && canWireConnectTo(level, x + 1, y - 1, z, -1)));
        bool on_zm = (canWireConnectTo(level, x, y, z - 1, 2) || (!level->isSolidRenderTile(x, y, z - 1) && canWireConnectTo(level, x, y - 1, z - 1, -1)));
        bool on_zp = (canWireConnectTo(level, x, y, z + 1, 0) || (!level->isSolidRenderTile(x, y, z + 1) && canWireConnectTo(level, x, y - 1, z + 1, -1)));
        if (!level->isSolidRenderTile(x, y + 1, z)) {
            if (level->isSolidRenderTile(x - 1, y, z) && canWireConnectTo(level, x - 1, y + 1, z, -1)) on_xm = true;
            if (level->isSolidRenderTile(x + 1, y, z) && canWireConnectTo(level, x + 1, y + 1, z, -1)) on_xp = true;
            if (level->isSolidRenderTile(x, y, z - 1) && canWireConnectTo(level, x, y + 1, z - 1, -1)) on_zm = true;
            if (level->isSolidRenderTile(x, y, z + 1) && canWireConnectTo(level, x, y + 1, z + 1, -1)) on_zp = true;
        }
        if (!on_zm && !on_xp && !on_xm && !on_zp && side >= 2 && side <= 5) return true;
        if (side == 2 && on_zm && !on_xm && !on_xp) return true;
        if (side == 3 && on_zp && !on_xm && !on_xp) return true;
        if (side == 4 && on_xm && !on_zm && !on_zp) return true;
        if (side == 5 && on_xp && !on_zm && !on_zp) return true;
        return false;
    }

    bool getDirectSignal(Level *level, int x, int y, int z, int direction) override {
        return checking_power ? false : getSignal2((LevelSource *) level, x, y, z, direction);
    }
};

static void make_repeater();
void make_redstone_wire() {
    // Redstone blocks
    redstone_wire = (new RedstoneWire(55, 164, Material::glass))->self;

    // Init
    redstone_wire->init();
    redstone_wire->setDestroyTime(0.0f);
    redstone_wire->setExplodeable(0.0f);
    redstone_wire->category = 4;
    redstone_wire->setDescriptionId("redstone");
    redstone_wire->setShape(0.0f, 0.0f, 0.0f, 1.0f, 0.0625f, 1.0f);

    make_repeater();
    make_redstone_tileitems();
}

// Repeater
bool repeater_rendering_torches = false;
struct BaseRepeater : CustomTile {
    BaseRepeater(const int id, const int texture, const Material *material): CustomTile(id, texture, material) {}

    AABB *getAABB(UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) override {
        // This is wrong, but I don't care
        return NULL;
    }
    int getRenderShape() override {
        return 53;
    }
    bool isSolidRender() override {
        return false;
    }
    bool isCubeShaped() override {
        return false;
    }
    bool mayPlace2(Level *level, int x, int y, int z) override {
        return canBePlacedOn(level, x, y - 1, z);
    }

    int getRenderLayer() override {
        return 1;
    }
    int getTexture1(int face) override {
        if (repeater_rendering_torches) {
            return self->id == active_repeater->id ? 3+16*6 : 3+16*7;
        }
        return self->texture;
    }

    static bool checkPower(Level *level, int x, int y, int z, int data = -1) {
        if (data == -1) data = level->getData(x, y, z);
        data &= 0b11;
        static constexpr int oxzs[][3] = {
            {0, 1, 3},
            {-1, 0, 4},
            {0, -1, 2},
            {1, 0, 5}
        };
        x += oxzs[data][0];
        z += oxzs[data][1];
        if (level->getSignal(x, y, z, oxzs[data][2])) {
            return true;
        }
        return level->getTile(x, y, z) == 55 && level->getData(x, y, z);
    }

    void neighborChanged(Level *level, int x, int y, int z, UNUSED int neighborId) override {
        if (!mayPlace2(level, x, y, z)) {
            level->setTile(x, y, z, 0);
            // TODO drop on ground
            return;
        }
        // Delay
        int data = level->getData(x, y, z);
        bool is_power = self->id == active_repeater->id;
        bool may_power = checkPower(level, x, y, z, data);
        if (is_power != may_power) {
            int delay = (data & 0b1100) >> 1;
            level->addToTickNextTick(x, y, z, self->id, delay + 2);
        }
    }

    void tick(Level *level, int x, int y, int z, UNUSED Random *random) override {
        int data = level->getData(x, y, z);
        bool is_power = self->id == active_repeater->id;
        bool may_power = checkPower(level, x, y, z, data);
        if (is_power && !may_power) {
            level->setTileAndData(x, y, z, inactive_repeater->id, data);
        } else if (!is_power) {
            level->setTileAndData(x, y, z, active_repeater->id, data);
            // Just because it isn't powered doesn't mean it wasn't
            // and if it was then it should, because then it is.
            // Then this makes it that it is, so then later check
            // the should and is, if not it can always be turned off
            if (!may_power) {
                int delay = (data & 0b1100) >> 1;
                level->addToTickNextTick(x, y, z, active_repeater->id, delay + 2);
            }
        }
    }

    void onPlace(Level *level, int x, int y, int z) override {
        level->updateNearbyTiles(x + 1, y, z, self->id);
        level->updateNearbyTiles(x - 1, y, z, self->id);
        level->updateNearbyTiles(x, y, z + 1, self->id);
        level->updateNearbyTiles(x, y, z - 1, self->id);
        level->updateNearbyTiles(x, y - 1, z, self->id);
        level->updateNearbyTiles(x, y + 1, z, self->id);
    }

    void setPlacedBy(Level *level, int x, int y, int z, Mob *placer) override {
        int face = (int) (std::floor((placer->yaw * 4.0) / 360.0 + 0.5) + 2) & 3;
        if (face == 0) {
            face = 4;
        } else if (face == 1) {
            face = 5;
        } else if (face == 2) {
            face = 2;
        } else if (face == 3) {
            face = 3;
        }
        level->setData(x, y, z, face);
        // Check power
        if (checkPower(level, x, y, z, level->getData(x, y, z))) {
            level->addToTickNextTick(x, y, z, active_repeater->id, 1);
        }
    }

    bool use(Level *level, int x, int y, int z, UNUSED Player *player) override {
        int dir = level->getData(x, y, z);
        int power = dir >> 2;
        power = (power + 1) & 0b11;
        dir = (dir & 0b0011) | (power << 2);
        level->setData(x, y, z, dir);
        return true;
    };

    int getResource(UNUSED int data, UNUSED Random *random) override {
        return REPEATER_ID;
    }
};

struct Repeater final : BaseRepeater {
    Repeater(const int id, const int texture, const Material *material): BaseRepeater(id, texture, material) {}
};

struct ActiveRepeater final : BaseRepeater {
    ActiveRepeater(const int id, const int texture, const Material *material): BaseRepeater(id, texture, material) {}

    bool getSignal2(LevelSource *level, int x, int y, int z, int side) override {
        int dir = level->getData(x, y, z) & 0b0011;
        return (dir == 0 && side == 3)
            || (dir == 1 && side == 4)
            || (dir == 2 && side == 2)
            || (dir == 3 && side == 5);
    }

    bool getDirectSignal(Level *level, int x, int y, int z, int direction) override {
        return getSignal2((LevelSource *) level, x, y, z, direction);
    }
};

static void make_repeater() {
    // Redstone blocks
    inactive_repeater = (new Repeater(93, 131, Material::glass))->self;
    active_repeater = (new ActiveRepeater(94, 147, Material::glass))->self;

    // Init
    inactive_repeater->init();
    active_repeater  ->init();
    inactive_repeater->setDestroyTime(0.0f);
    active_repeater  ->setDestroyTime(0.0f);
    inactive_repeater->setExplodeable(0.0f);
    active_repeater  ->setExplodeable(0.0f);
    inactive_repeater->category = 4;
    active_repeater  ->category = 4;
    std::string name = "repeater";
    inactive_repeater->setDescriptionId(name);
    active_repeater  ->setDescriptionId(name);
    inactive_repeater->setShape(0.0f, 0.0f, 0.0f, 1.0f, 0.0625f, 1.0f);
    active_repeater  ->setShape(0.0f, 0.0f, 0.0f, 1.0f, 0.0625f, 1.0f);
}


// Redstone block
struct BaseRedstoneBlock : CustomTile {
    BaseRedstoneBlock(const int id, const int texture, const Material *material): CustomTile(id, texture, material) {}

    bool isSignalSource() override {
        return true;
    }

    bool getSignal(UNUSED LevelSource *level, UNUSED int x, UNUSED int y, UNUSED int z) override {
        return true;
    }

    bool getSignal2(UNUSED LevelSource *level, UNUSED int x, UNUSED int y, UNUSED int z, UNUSED int direction) override {
        return true;
    }
};

struct RedstoneBlock final : BaseRedstoneBlock {
    RedstoneBlock(const int id, const int texture, const Material *material): BaseRedstoneBlock(id, texture, material) {}
};

struct ActiveRedstoneBlock final : BaseRedstoneBlock {
    ActiveRedstoneBlock(const int id, const int texture, const Material *material): BaseRedstoneBlock(id, texture, material) {}

    bool getDirectSignal(UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z, UNUSED int direction) override {
        return true;
    }

    void onPlaceOrRemove(Level *level, int x, int y, int z) {
        level->updateNearbyTiles(x, y - 1, z, self->id);
        level->updateNearbyTiles(x, y + 1, z, self->id);
        level->updateNearbyTiles(x - 1, y, z, self->id);
        level->updateNearbyTiles(x + 1, y, z, self->id);
        level->updateNearbyTiles(x, y, z - 1, self->id);
        level->updateNearbyTiles(x, y, z + 1, self->id);
    }
    void onPlace(Level *level, int x, int y, int z) override {
        onPlaceOrRemove(level, x, y, z);
    }
    void onRemove(Level *level, int x, int y, int z) override {
        onPlaceOrRemove(level, x, y, z);
    }
};

void make_redstone_torch();
void make_redstone_block() {
    redstone_block = (new RedstoneBlock(152, 173, Material::glass))->self;
    active_redstone_block = (new ActiveRedstoneBlock(153, 191, Material::glass))->self;

    // Init
    redstone_block->init();
    active_redstone_block->init();
    redstone_block->setDestroyTime(2.0f);
    active_redstone_block->setDestroyTime(2.0f);
    redstone_block->setExplodeable(10.0f);
    active_redstone_block->setExplodeable(10.0f);
    redstone_block->setSoundType(Tile::SOUND_STONE);
    active_redstone_block->setSoundType(Tile::SOUND_STONE);
    redstone_block->category = 4;
    active_redstone_block->category = 4;
    std::string rb_name = "redstone_block";
    redstone_block->setDescriptionId(rb_name);
    std::string arb_name = "active_redstone_block";
    active_redstone_block->setDescriptionId(arb_name);
}
void make_redstone_blocks() {
    // Blocks with redstone
    make_redstone_torch();
    make_redstone_block();
}

struct RedstoneTorch final : CustomTile {
    RedstoneTorch(const int id, const int texture, const Material *material): CustomTile(id, texture, material) {}

    bool isSignalSource() override {
        return true;
    }

    bool getSignal2(LevelSource *level, int x, int y, int z, int side) override {
        int data = level->getData(x, y, z);
        // Inactive torch
        if (data & 0b1000) return false;
        // Check side
        if (data == 6 - side) return false;
        return true;
    }

    bool getDirectSignal(Level *level, int x, int y, int z, int direction) override {
        return direction ? false : getSignal2((LevelSource *) level, x, y, z, direction);
    }


    #if 0
    static struct TorchInfo {
        int x, y, z;
        int time;
    };

    static std::vector<TorchInfo>
    void tick(Level *level, int x, int y, int z) override {
        // Remove from list
        int time = level->levelData->time;
        while (torchUpdates.size() > 0 && time - torchUpdate.at(0).time > 10) {
            torchUpdates.remove(0);
        }
    #else
    void tick(Level *level, int x, int y, int z, UNUSED Random *random) override {
    #endif
        // Check if powered
        int data = level->vtable->getData(level, x, y, z);
        data &= 0b0111;
        int dm6x = (6 - data) ^ 1;
        bool powered = (data == 1 && level->getSignal(x - 1, y, z, dm6x))
            || (data == 2 && level->getSignal(x + 1, y, z, dm6x))
            || (data == 3 && level->getSignal(x, y, z - 1, dm6x))
            || (data == 4 && level->getSignal(x, y, z + 1, dm6x))
            || (data == 5 && level->getSignal(x, y - 1, z, dm6x));
        // Set data
        data |= powered << 3;
        if (level->getData(x, y, z) != data) {
            level->setTileAndData(x, y, z, self->id, data);
        }
    }

    void neighborChanged(Level *level, int x, int y, int z, UNUSED int neighborId) override {
        level->addToTickNextTick(x, y, z, self->id, 20);
    }

    void onPlaceOrRemove(Level *level, int x, int y, int z) {
        int data = level->getData(x, y, z);
        data &= 0b1000;
        if (!data) return;
        level->updateNearbyTiles(x, y - 1, z, self->id);
        level->updateNearbyTiles(x, y + 1, z, self->id);
        level->updateNearbyTiles(x - 1, y, z, self->id);
        level->updateNearbyTiles(x + 1, y, z, self->id);
        level->updateNearbyTiles(x, y, z - 1, self->id);
        level->updateNearbyTiles(x, y, z + 1, self->id);
    }
    void onPlace(Level *level, int x, int y, int z) override {
        onPlaceOrRemove(level, x, y, z);
    }
    void onRemove(Level *level, int x, int y, int z) override {
        onPlaceOrRemove(level, x, y, z);
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
        if (placer->pitch < -55) face = 1;
        level->setData(x, y, z, face);
    }

    int getTexture1(int face) override {
        return 3+16*6;
    }

    int getTexture2(UNUSED int face, int data) override {
        // Shape 2 doesn't respect this
        if (data & 0b1000) return 3+16*7;
        return 3+16*6;
    }

    int getTexture3(LevelSource *level, int x, int y, int z, int face) override {
        return getTexture2(face, level->getData(x, y, z));
    }
    AABB *getAABB(UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) override {
        return NULL;
    }
    bool isSolidRender() override {
        return false;
    }
    bool isCubeShaped() override {
        return false;
    }
    int getRenderLayer() override {
        return 1;
    }
    int getRenderShape() {
        return 2;
    }
};

void make_redstone_torch() {
    // Redstone blocks
    redstone_torch = (new RedstoneTorch(75, 3+16*7, Material::wood))->self;

    // Init
    redstone_torch->init();
    redstone_torch->setDestroyTime(0.0f);
    redstone_torch->setExplodeable(0.0f);
    redstone_torch->category = 4;
    std::string name = "redstone_torch";
    redstone_torch->setDescriptionId(name);

}

struct Lamp final : CustomTile {
    Lamp(const int id, const int texture, const Material *material): CustomTile(id, texture, material) {}

    void neighborChanged(Level *level, int x, int y, int z, UNUSED int neighborId) override {
        bool powered = level->hasDirectSignal(x, y, z) || level->hasNeighborSignal(x, y, z);
        if (!powered && self->id == active_lamp->id) {
            level->setTile(x, y, z, lamp->id);
        } else if (powered && self->id == lamp->id) {
            level->setTile(x, y, z, active_lamp->id);
        }
    }

    void onPlace(Level *level, int x, int y, int z) override {
        neighborChanged(level, x, y, z, 0);
    }
};

void make_lamp(int id, bool active) {
    Tile *new_lamp = (new Lamp(id, (11+16*11) + active, Material::glass))->self;
    if (active) Tile::lightEmission[id] = 300;

    new_lamp->init();
    new_lamp->setDestroyTime(0.3f);
    new_lamp->category = 4;

    if (active) {
        active_lamp = new_lamp;
    } else {
        lamp = new_lamp;
    }
}

bool Level_getSignal_isSolidBlockingTile_injection(Level *level, int x, int y, int z) {
    int id = level->getTile(x, y, z);
    if (id == 152 /* Redstone Block */) return false;
    // Call original
    return level->isSolidBlockingTile(x, y, z);
}

__attribute__((constructor)) static void init() {
    // Make the ore drop it
    patch_address((void *) 0x113a38, (void *) RedStoneOreTile_getResource_injection);
    // Fix redstone blocks providing signals
    overwrite_call((void *) 0xa5e8c, Level_isSolidBlockingTile, Level_getSignal_isSolidBlockingTile_injection);
    // Fix shape 2 being stupid
    //overwrite_call((void *) 0x54660);
}
