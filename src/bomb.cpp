#include <math.h>

//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "bomb.h"

bool really_hit(Throwable *self, HitResult *res) {
    if (res->type == 0) {
        // Check for solid tile
        Tile *t = Tile::tiles[self->level->getTile(res->x, res->y, res->z)];
        return t && t->getAABB(self->level, res->x, res->y, res->z) != NULL;
    } else if (res->type == 1) {
        // Entites
        return true;
    } else {
        return false;
    }
}

struct Bomb final : CustomThrowable {
    Bomb(Level *level, Entity *thrower) : CustomThrowable(level, thrower) {
        self->renderer_id = BOMB_RENDER_DISPATCHER_ID;
    }

    void onHit(HitResult *res) override {
        if (!really_hit(self, res)) return;

        // Sound
        float f = Mth::_random.genrand_int32();
        std::string name = "random.explode";
        self->level->playSound((Entity *) self, name, 0.5, 0.4 / (f * 0.4 + 0.8));

        // Particle
        std::string part = "explode";
        self->level->addParticle(part, self->x, self->y, self->z, 0, 0, 0, 10);

        // Explode
        constexpr float radius = 5;
        constexpr float blast_back = 2;
        AABB aabb{self->x - radius, self->y - radius, self->z - radius, self->x + radius, self->y + radius, self->z + radius};
        std::vector<Entity *> *entities = self->level->getEntities((Entity *) self, aabb);
        for (Entity *e : *entities) {
            if (!e || (e->isMob() && ((Mob *) e)->health <= 0)) continue;
            // Get dist
            float xd = e->x - self->x;
            float yd = e->y - self->y;
            float zd = e->z - self->z;
            float dist = sqrt(xd * xd + yd * yd + zd * zd);
            float explosion_dist = dist / radius;
            if (1.f >= explosion_dist) {
                // Blast back
                xd /= dist;
                yd /= dist;
                zd /= dist;
                // TODO: Get the real seen any%
                float seen = (1.f - explosion_dist) * 1.0;
                e->velocity_x += xd * blast_back * seen;
                e->velocity_y += yd * blast_back * seen;
                e->velocity_z += zd * blast_back * seen;
                // Fix fall dist
                e->fall_distance = 0;
            }
        }

        // Remove
        self->remove();
    }

    void tick() override {
        std::string part = "smoke";
        self->level->addParticle(part, self->x, self->y, self->z, 0, 0, 0, 1);
        Throwable_tick->get(false)(self);
    }

    int getEntityTypeId() override {
        return BOMB_ID;
    }
};

struct BombItem final : CustomItem {
    BombItem(int id) : CustomItem(id) {}

    ItemInstance *use(ItemInstance *item, Level *level, Player *player) override {
        // Deplete
        if (!player->abilities.infinite_items) {
            item->count -= 1;
        }

        // Play the sounds
        float f = Mth::_random.genrand_int32();
        std::string name = "step.cloth";
        level->playSound((Entity *) player, name, 0.5, 0.4 / (f * 0.4 + 0.8));

        // Spawn the entity
        level->addEntity((Entity *) (new Bomb(level, (Entity *) player))->self);

        return item;
    }
};

// Create Item
static Item *bomb = NULL;
void make_bomb() {
    // Construct
    bomb = (new BombItem(BOMB_ITEM_ID - 256))->self;

    // Setup
    std::string name = "bomb";
    bomb->setDescriptionId(name);
    bomb->category = 2;
    bomb->max_damage = 0;
    bomb->max_stack_size = 16;
    bomb->texture = BOMB_TEXTURE_ID;
}

OVERWRITE_CALLS(
    EntityRenderDispatcher_constructor,
    EntityRenderDispatcher *, EntityRenderDispatcher_constructor_injection, (EntityRenderDispatcher_constructor_t original, EntityRenderDispatcher *self)
) {
    original(self);

    ItemSpriteRenderer *renderer = ItemSpriteRenderer::allocate();
    renderer->constructor(BOMB_TEXTURE_ID);
    self->assign(BOMB_RENDER_DISPATCHER_ID, (EntityRenderer *) renderer);

    return self;
}
