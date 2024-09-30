#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "ender_pearls.h"
#include "bomb.h"

static void EnderPearl_onHit(Throwable *self, UNUSED HitResult *res) {
    if (!really_hit(self, res)) return;
    self->remove();

    // Teleport
    Entity *e = self->level->getEntity(self->thrower_id);
    if (!e) return;
    e->moveTo(self->x, self->y, self->z, e->yaw, e->pitch);
    e->hurt(nullptr, 4);
}

static int EnderPearl_getEntityTypeId(UNUSED Throwable *self) {
    return ENDER_PEARL_ID;
}

static Throwable_vtable *get_ender_pearl_vtable() {
    static Throwable_vtable *vtable = NULL;
    if (vtable == NULL) {
        // Init
        vtable = dup_vtable(Throwable_vtable_base);
        ALLOC_CHECK(vtable);

        // Modify
        vtable->onHit = EnderPearl_onHit;
        vtable->getEntityTypeId = EnderPearl_getEntityTypeId;
    }
    return vtable;
}

ItemInstance *EnderPearlItem_use(UNUSED Item *self, ItemInstance *item, Level *level, Player *player) {
    // Deplete
    if (!player->infinite_items) {
        item->count -= 1;
    }

    // Play the sounds
    float f = Mth::_random.genrand_int32();
    std::string name = "random.bow";
    //std::string name = "jonker";
    level->playSound((Entity *) player, name, 0.5, 0.4 / (f * 0.4 + 0.8));

    // Spawn the entity
    Throwable *ender_pearl = Throwable::allocate();
    ALLOC_CHECK(ender_pearl);
    ender_pearl->constructor(level, (Entity *) player);
    ender_pearl->vtable = get_ender_pearl_vtable();
    ender_pearl->renderer_id = PEARL_RENDER_DISPATCHER_ID;
    level->addEntity((Entity *) ender_pearl);

    return item;
}

static Item_vtable *get_ender_pearl_item_vtable() {
    static Item_vtable *vtable = NULL;
    if (vtable == NULL) {
        // Init
        vtable = dup_vtable(Item_vtable_base);
        ALLOC_CHECK(vtable);

        // Modify
        vtable->use = EnderPearlItem_use;
    }
    return vtable;
}

// Create Item
static Item *ender_pearl = NULL;
void make_ender_pearl() {
    // Construct
    ender_pearl = Item::allocate();
    ALLOC_CHECK(ender_pearl);
    ender_pearl->constructor(ENDER_PEARL_ITEM_ID - 256);

    // Set VTable
    ender_pearl->vtable = get_ender_pearl_item_vtable();

    // Setup
    std::string name = "ender_pearl";
    ender_pearl->setDescriptionId(name);
    ender_pearl->max_damage = 0;
    ender_pearl->max_stack_size = 16;
    ender_pearl->texture = ENDER_PEARL_TEXTURE_ID;
}

OVERWRITE_CALLS(EntityRenderDispatcher_constructor, EntityRenderDispatcher*, EntityRenderDispatcher_constructor_injection, (EntityRenderDispatcher_constructor_t original, EntityRenderDispatcher *self)) {
    original(self);

    ItemSpriteRenderer *renderer = ItemSpriteRenderer::allocate();
    ALLOC_CHECK(renderer);
    renderer->constructor(ENDER_PEARL_TEXTURE_ID);
    self->assign(PEARL_RENDER_DISPATCHER_ID, (EntityRenderer *) renderer);

    return self;
}
