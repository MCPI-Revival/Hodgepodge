#include <math.h>

#include <libreborn/libreborn.h>
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

static void Bomb_onHit(Throwable *self, HitResult *res) {
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
            e->vel_x += xd * blast_back * seen;
            e->vel_y += yd * blast_back * seen;
            e->vel_z += zd * blast_back * seen;
            // Fix fall dist
            e->fall_distance = 0;
        }
    }

    // Remove
    self->remove();
}

static void (*super_tick)(Throwable *) = NULL;
static void Bomb_tick(Throwable *self) {
    std::string part = "smoke";
    self->level->addParticle(part, self->x, self->y, self->z, 0, 0, 0, 1);
    super_tick(self);
}

static int Bomb_getEntityTypeId(UNUSED Throwable *self) {
    return BOMB_ID;
}

static Throwable_vtable *get_bomb_vtable() {
    static Throwable_vtable *vtable = NULL;
    if (vtable == NULL) {
        // Init
        vtable = dup_vtable(Throwable_vtable_base);
        ALLOC_CHECK(vtable);

        // Modify
        vtable->onHit = Bomb_onHit;
        super_tick = vtable->tick;
        vtable->tick = Bomb_tick;
        vtable->getEntityTypeId = Bomb_getEntityTypeId;
    }
    return vtable;
}

static ItemInstance *BombItem_use(UNUSED Item *self, ItemInstance *item, Level *level, Player *player) {
    // Deplete
    if (!player->infinite_items) {
        item->count -= 1;
    }

    // Play the sounds
    float f = Mth::_random.genrand_int32();
    std::string name = "step.cloth";
    level->playSound((Entity *) player, name, 0.5, 0.4 / (f * 0.4 + 0.8));

    // Spawn the entity
    Throwable *bomb = Throwable::allocate();
    ALLOC_CHECK(bomb);
    bomb->constructor(level, (Entity *) player);
    bomb->vtable = get_bomb_vtable();
    bomb->renderer_id = BOMB_RENDER_DISPATCHER_ID;
    level->addEntity((Entity *) bomb);

    return item;
}

static Item_vtable *get_bomb_item_vtable() {
    static Item_vtable *vtable = NULL;
    if (vtable == NULL) {
        // Init
        vtable = dup_vtable(Item_vtable_base);
        ALLOC_CHECK(vtable);

        // Modify
        vtable->use = BombItem_use;
    }
    return vtable;
}

// Create Item
static Item *bomb = NULL;
void make_bomb() {
    // Construct
    bomb = Item::allocate();
    ALLOC_CHECK(bomb);
    bomb->constructor(BOMB_ITEM_ID - 256);

    // Set VTable
    bomb->vtable = get_bomb_item_vtable();

    // Setup
    std::string name = "bomb";
    bomb->setDescriptionId(name);
    bomb->category = 2;
    bomb->max_damage = 0;
    bomb->max_stack_size = 16;
    bomb->texture = BOMB_TEXTURE_ID;
}

OVERWRITE_CALLS(EntityRenderDispatcher_constructor, EntityRenderDispatcher*, EntityRenderDispatcher_constructor_injection, (EntityRenderDispatcher_constructor_t original, EntityRenderDispatcher *self)) {
    original(self);

    ItemSpriteRenderer *renderer = ItemSpriteRenderer::allocate();
    ALLOC_CHECK(renderer);
    renderer->constructor(BOMB_TEXTURE_ID);
    self->assign(BOMB_RENDER_DISPATCHER_ID, (EntityRenderer *) renderer);

    return self;
}
