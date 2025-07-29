//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "ender_pearls.h"
#include "bomb.h"

struct EnderPearl final : CustomThrowable {
    EnderPearl(Level *level, Entity *thrower) : CustomThrowable(level, thrower) {
        this->self->renderer_id = PEARL_RENDER_DISPATCHER_ID;
    }

    void onHit(HitResult *res) override {
        if (!really_hit(self, res)) return;
        self->remove();

        // Teleport
        Entity *e = self->level->getEntity(self->thrower_id);
        if (!e) return;
        e->moveTo(self->x, self->y, self->z, e->yaw, e->pitch);
        e->hurt(nullptr, 4);
    }

    int getEntityTypeId() override {
        return ENDER_PEARL_ID;
    }
};

struct EnderPearlItem final : CustomItem {
    EnderPearlItem(int id) : CustomItem(id) {}

    ItemInstance *use(ItemInstance *item, Level *level, Player *player) override {
        // Deplete
        if (!player->abilities.infinite_items) {
            item->count -= 1;
        }

        // Play the sounds
        float f = Mth::_random.genrand_int32();
        std::string name = "random.bow";
        level->playSound((Entity *) player, name, 0.5, 0.4 / (f * 0.4 + 0.8));

        // Spawn the entity
        level->addEntity((Entity *) (new EnderPearl(level, (Entity *) player))->self);

        return item;
    }
};

// Create Item
static Item *ender_pearl = NULL;
void make_ender_pearl() {
    // Construct
    ender_pearl = (new EnderPearlItem(ENDER_PEARL_ITEM_ID - 256))->self;

    // Setup
    std::string name = "ender_pearl";
    ender_pearl->setDescriptionId(name);
    ender_pearl->max_damage = 0;
    ender_pearl->max_stack_size = 16;
    ender_pearl->texture = ENDER_PEARL_TEXTURE_ID;
}

OVERWRITE_CALLS(
    EntityRenderDispatcher_constructor,
    EntityRenderDispatcher *, EntityRenderDispatcher_constructor_injection, (EntityRenderDispatcher_constructor_t original, EntityRenderDispatcher *self)
) {
    original(self);

    ItemSpriteRenderer *renderer = ItemSpriteRenderer::allocate();
    renderer->constructor(ENDER_PEARL_TEXTURE_ID);
    self->assign(PEARL_RENDER_DISPATCHER_ID, (EntityRenderer *) renderer);

    return self;
}
