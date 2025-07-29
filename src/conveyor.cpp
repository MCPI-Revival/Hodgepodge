#include <cmath>

//#include <libreborn/libreborn.h>
#include <libreborn/patch.h>
#include <symbols/minecraft.h>

#include "api.h"
#include "belt.h"

static Tile *conveyorbelt = NULL;
struct ConveyorBelt final : CustomTile {
    ConveyorBelt(const int id, const int texture, const Material *material): CustomTile(id, texture, material) {}

    // Item Textures
    int getTexture2(UNUSED int face, UNUSED int data) override {
        return BELT_ITEM_TEXTURE;
    }

    // Rendering
    bool isSolidRender() override {
        return 0;
    }
    int getRenderLayer() override {
        return 1;
    }
    bool isCubeShaped() override {
        return false;
    }
    int getRenderShape() override {
        return 47;
    }

    // Size
    void updateDefaultShape() override {
        // Set the default shape
        self->setShape(
            0.0, 0.0, 0.0,
            1.0, 0.5, 1.0
        );
    }

    AABB *getAABB(UNUSED Level *level, int x, int y, int z) override {
        // Corner 1
        AABB *aabb = &self->aabb;
        aabb->x1 = (float) x;
        aabb->y1 = (float) y;
        aabb->z1 = (float) z;

        // Corner 2
        aabb->x2 = (float) x + 1.0;
        aabb->y2 = (float) y + 0.5;
        aabb->z2 = (float) z + 1.0;

        return aabb;
    }

    void updateShape(UNUSED LevelSource *level, UNUSED int x, UNUSED int y, UNUSED int z) override {
        updateDefaultShape();
    }

    // Direction
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
        } else {
            face = 0;
        }
        level->setTileAndData(x, y, z, self->id, face);
    }

    // Interaction
    bool use(Level *level, int x, int y, int z, Player *player) override {
        AABB aabb{(float) x, (float) y, (float) z, (float) x, (float) y, (float) z};
        std::vector<Entity *> buff = {};
        int num_removed = 0;
        if (!level->getEntitiesOfType(64, aabb, buff)) return false;
        // Remove items and give them to player
        for (Entity *e : buff) {
            // TODO: Check MP support
            if ((int)e->x == x && (int)e->y == y && (int)e->z == z) {
                num_removed += 1;
                Inventory *inventory = player->inventory;
                if (inventory->add(&((ItemEntity*) e)->item)) {
                    // Don't remove if it wasn't given fully to the player
                    e->remove();
                }
            }
        }
        return num_removed != 0;
    }

    void entityInside(Level *level, int x, int y, int z, Entity *entity) override {
        constexpr float mov_speed = 0.1f;
        int data = level->getData(x, y, z);
        if (data == 2) {
            entity->velocity_z = std::min(entity->velocity_z, -mov_speed);
        } else if (data == 3) {
            entity->velocity_z = std::max(entity->velocity_z, mov_speed);
        } else if (data == 4) {
            entity->velocity_x = std::min(entity->velocity_x, -mov_speed);
        } else if (data == 5) {
            entity->velocity_x = std::max(entity->velocity_x, mov_speed);
        } else {
            // We do a little trolling
            entity->velocity_y = std::max(entity->velocity_y, 0.5f);
        }
        // Don't let items despawn on belts
        if (entity->getEntityTypeId() == 64) {
            ((ItemEntity *) entity)->age = 0;
            entity->y = std::max((float) y + 0.5f, entity->y);
        }
    }
};

OVERWRITE_CALLS(ItemEntity_playerTouch, void, ItemEntity_playerTouch_injection, (ItemEntity_playerTouch_t original, ItemEntity *self, Player *player)) {
    int tile_id = self->level->getTile(self->x, self->y, self->z);
    // Disallow item pickup if it's on a belt or going on to one
    if (tile_id != BELT_ID) {
        original(self, player);
    }
}

// Makes the conveyorbelts
void make_conveyorbelt() {
    // Construct
    conveyorbelt = (new ConveyorBelt(BELT_ID, BELT_ITEM_TEXTURE, Material::metal))->self;
    conveyorbelt->updateDefaultShape();

    // Init
    conveyorbelt->init();
    conveyorbelt->setDestroyTime(2.0f);
    conveyorbelt->setExplodeable(10.0f);
    conveyorbelt->category = 4;
    std::string name = "ConveyorBelt";
    conveyorbelt->setDescriptionId(name);
}
