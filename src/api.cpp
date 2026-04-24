#include <symbols/ItemEntity.h>
#include <symbols/EntityFactory.h>

#include "api.h"

CompoundTag *CompoundTag_getCompound_but_not(CompoundTag *self, const std::string &name) {
    std::string _name = name;
    if (!self->contains(_name, 10)) return NULL;
    return (CompoundTag *) (int) self->getShort(_name);
}

void summon_item(Level *level, float x, float y, float z, ItemInstance item) {
    ItemEntity *item_entity = (ItemEntity *) EntityFactory::CreateEntity(64, level);
    item_entity->constructor(level, x, y, z, item);
    item_entity->moveTo(x, y, z, 0, 0);
    level->addEntity((Entity *) item_entity);
}
