#include <dlfcn.h>

#include "api.h"

CompoundTag *CompoundTag_getCompound_but_not(CompoundTag *self, const std::string &name) {
    std::string _name = name;
    if (!self->contains(_name, 10)) return NULL;
    return (CompoundTag *) (int) self->getShort(_name);
}

#ifdef FISH
// The most important function
bool is_fish() {
    static signed char t = -1;
    if (t != -1) return t == 1;
    // Check
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    t = (timeinfo->tm_mday == 1 && timeinfo->tm_mon == 3);
    // Return
    return t == 1;
}
#endif