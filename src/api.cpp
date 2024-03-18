#include "api.h"

CompoundTag *CompoundTag_getCompound_but_not(CompoundTag *self, const std::string &name) {
    std::string _name = name;
    if (!CompoundTag_contains(self, &_name, 10)) return NULL;
    return (CompoundTag *) (int) CompoundTag_getShort(self, &_name);
}
