#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>

#include "api.h"

static int get_sign(int i) {
    if (i == 0) return 0;
    if (i < 0) return -1;
    return 1;
}

static bool is_food(int entity_type, Item *item) {
    if (entity_type == 10) {
        // Chickens like seeds
        return item->id == 362 || item->id == 295;
    } else if (entity_type == 11 || entity_type == 13) {
        // Cows and sheep like wheat
        return item->id == 296;
    } else if (entity_type == 12) {
        // Pigs like everything
        return item->id == 37 || item->id == 38 || item->id == 39 || item->id == 40 || item->id == 260 || item->id == 282 || item->id == 353;
    }
    // Some weird entity, I hope they like explosions!
    return item->id == 46;
}

static Entity *Animal_findAttackTarget_injection(Animal *self) {
    // Check if running away
    if (self->running_away_timer) return NULL;
    // Check noramally
    AABB aabb{self->hitbox};
    aabb.x1 -= get_sign(self->hitbox.x1) * 3;
    aabb.y1 -= get_sign(self->hitbox.y1) * 3;
    aabb.z1 -= get_sign(self->hitbox.z1) * 3;
    aabb.x2 += get_sign(self->hitbox.x2) * 3;
    aabb.y2 += get_sign(self->hitbox.y2) * 3;
    aabb.z2 += get_sign(self->hitbox.z2) * 3;
    std::vector<Entity *> buff = {};
    if (!self->level->getEntitiesOfType(0, aabb, buff)) return NULL;
    for (Entity *e : buff) {
        if (!e->isPlayer()) continue;
        Inventory *inventory = ((Player *) e)->inventory;
        ItemInstance *held_ii = inventory->getSelected();
        if (!held_ii) continue;
        Item *held = Item::items[held_ii->id];
        if (is_food(self->getEntityTypeId(), held)) {
            // They are holding food :)
            return e;
        }
    }
    return NULL;
}

void Animal_updateAi_injection(Animal *self) {
    // Check if the player is still holding the food and close enough
    if (self->target_id != 0) {
        Entity *e = self->level->getEntity(self->target_id);
        if (!e || !e->isPlayer()) {
            self->target_id = 0;
        } else {
            Inventory *inventory = ((Player *) e)->inventory;
            ItemInstance *held_ii = inventory->getSelected();
            if (!held_ii) {
                self->target_id = 0;
            } else {
                Item *held = Item::items[held_ii->id];
                self->target_id *= is_food(self->getEntityTypeId(), held);
            }
            // Check dist
            int dist = abs(self->x - e->x) + abs(self->z - e->z);
            if (dist >= 5) {
                self->target_id = 0;
            }
        }
    }
    // Check if running away, animals are not masochistic
    if (self->running_away_timer) {
        self->target_id = 0;
    }
    // Continue
    PathfinderMob_updateAi->get(false)((PathfinderMob *) self);
}

__attribute__((constructor)) static void init() {
    patch_vtable(Animal_findAttackTarget, Animal_findAttackTarget_injection);
    // Fix PathfinderMob::updateAi latching on
    for (void *i : {(void *) Chicken_updateAi->get_vtable_addr(), (void *) Cow_updateAi->get_vtable_addr(), (void *) Pig_updateAi->get_vtable_addr(), (void *) Sheep_updateAi->get_vtable_addr()}) {
        patch_address(i, (void *) Animal_updateAi_injection);
    }
}
