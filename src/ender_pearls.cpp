#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "ender_pearls.h"
#include "bomb.h"

static void EnderPearl_onHit(Throwable *self, UNUSED HitResult *res) {
    if (!really_hit(self, res)) return;
    self->vtable->remove(self);

    // Teleport
    Entity *e = Level_getEntity(self->level, self->thrower_id);
    if (!e) return;
    Entity_moveTo_non_virtual(e, self->x, self->y, self->z, e->yaw, e->pitch);
    e->vtable->hurt(e, NULL, 4);
}

static int EnderPearl_getEntityTypeId(UNUSED Throwable *self) {
    return ENDER_PEARL_ID;
}

static Throwable_vtable *get_ender_pearl_vtable() {
    static Throwable_vtable *vtable = NULL;
    if (vtable == NULL) {
        // Init
        vtable = dup_Throwable_vtable(Throwable_vtable_base);
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
    float f = Random_genrand_int32(&Mth__random);
    std::string name = "random.bow";
    //std::string name = "jonker";
    Level_playSound(level, (Entity *) player, &name, 0.5, 0.4 / (f * 0.4 + 0.8));

    // Spawn the entity
    Throwable *ender_pearl = alloc_Throwable();
    ALLOC_CHECK(ender_pearl);
    Throwable_constructor(ender_pearl, level, (Entity *) player);
    ender_pearl->vtable = get_ender_pearl_vtable();
    ender_pearl->renderer_id = PEARL_RENDER_DISPATCHER_ID;
    Level_addEntity(level, (Entity *) ender_pearl);

    return item;
}

static Item_vtable *get_ender_pearl_item_vtable() {
    static Item_vtable *vtable = NULL;
    if (vtable == NULL) {
        // Init
        vtable = dup_Item_vtable(Item_vtable_base);
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
    ender_pearl = alloc_Item();
    ALLOC_CHECK(ender_pearl);
    Item_constructor(ender_pearl, ENDER_PEARL_ITEM_ID - 256);

    // Set VTable
    ender_pearl->vtable = get_ender_pearl_item_vtable();

    // Setup
    std::string name = "ender_pearl";
    ender_pearl->vtable->setDescriptionId(ender_pearl, &name);
    ender_pearl->max_damage = 0;
    ender_pearl->max_stack_size = 16;
    ender_pearl->texture = ENDER_PEARL_TEXTURE_ID;
}

HOOK_FROM_CALL(0x60eb0, EntityRenderDispatcher*, EntityRenderDispatcher_constructor, (EntityRenderDispatcher *self)) {
    EntityRenderDispatcher_constructor_original(self);

    ItemSpriteRenderer *renderer = alloc_ItemSpriteRenderer();
    ALLOC_CHECK(renderer);
    ItemSpriteRenderer_constructor(renderer, ENDER_PEARL_TEXTURE_ID);
    EntityRenderDispatcher_assign(self, PEARL_RENDER_DISPATCHER_ID, (EntityRenderer *) renderer);

    return self;
}
