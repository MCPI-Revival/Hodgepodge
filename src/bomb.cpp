#include <math.h>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "bomb.h"

bool really_hit(Throwable *self, HitResult *res) {
    if (res->type == 0) {
        // Check for solid tile
        Tile *t = Tile_tiles[self->level->vtable->getTile(self->level, res->x, res->y, res->z)];
        return t && t->vtable->getAABB(t, self->level, res->x, res->y, res->z) != NULL;
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
    float f = Random_genrand_int32(&Mth__random);
    std::string name = "random.explode";
    Level_playSound(self->level, (Entity *) self, &name, 0.5, 0.4 / (f * 0.4 + 0.8));

    // Particle
    std::string part = "explode";
    Level_addParticle(self->level, &part, self->x, self->y, self->z, 0, 0, 0, 10);

    // Explode
    constexpr float radius = 5;
    constexpr float blast_back = 2;
    AABB aabb{self->x - radius, self->y - radius, self->z - radius, self->x + radius, self->y + radius, self->z + radius};
    std::vector<Entity *> *entities = Level_getEntities(self->level, (Entity *) self, &aabb);
    for (Entity *e : *entities) {
        if (!e || (e->vtable->isMob(e) && ((Mob *) e)->health <= 0)) continue;
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
    self->vtable->remove(self);
}

static Throwable_tick_t super_tick = NULL;
static void Bomb_tick(Throwable *self) {
    std::string part = "smoke";
    Level_addParticle(self->level, &part, self->x, self->y, self->z, 0, 0, 0, 1);
    super_tick(self);
}

static int Bomb_getEntityTypeId(UNUSED Throwable *self) {
    return BOMB_ID;
}

static Throwable_vtable *get_bomb_vtable() {
    static Throwable_vtable *vtable = NULL;
    if (vtable == NULL) {
        // Init
        vtable = dup_Throwable_vtable(Throwable_vtable_base);
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
    float f = Random_genrand_int32(&Mth__random);
    std::string name = "step.cloth";
    Level_playSound(level, (Entity *) player, &name, 0.5, 0.4 / (f * 0.4 + 0.8));

    // Spawn the entity
    Throwable *bomb = new Throwable();
    ALLOC_CHECK(bomb);
    Throwable_constructor(bomb, level, (Entity *) player);
    bomb->vtable = get_bomb_vtable();
    bomb->renderer_id = BOMB_RENDER_DISPATCHER_ID;
    Level_addEntity(level, (Entity *) bomb);

    return item;
}

static Item_vtable *get_bomb_item_vtable() {
    static Item_vtable *vtable = NULL;
    if (vtable == NULL) {
        // Init
        vtable = dup_Item_vtable(Item_vtable_base);
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
    bomb = new Item();
    ALLOC_CHECK(bomb);
    Item_constructor(bomb, BOMB_ITEM_ID - 256);

    // Set VTable
    bomb->vtable = get_bomb_item_vtable();

    // Setup
    std::string name = "bomb";
    bomb->vtable->setDescriptionId(bomb, &name);
    bomb->category = 2;
    bomb->max_damage = 0;
    bomb->max_stack_size = 16;
    bomb->texture = BOMB_TEXTURE_ID;
}

HOOK_FROM_CALL(0x60eb0, EntityRenderDispatcher*, EntityRenderDispatcher_constructor, (EntityRenderDispatcher *self)) {
    EntityRenderDispatcher_constructor_original(self);

    ItemSpriteRenderer *renderer = new ItemSpriteRenderer();
    ALLOC_CHECK(renderer);
    ItemSpriteRenderer_constructor(renderer, BOMB_TEXTURE_ID);
    EntityRenderDispatcher_assign(self, BOMB_RENDER_DISPATCHER_ID, (EntityRenderer *) renderer);

    return self;
}
