#include <dlfcn.h>

#include "api.h"

CompoundTag *CompoundTag_getCompound_but_not(CompoundTag *self, const std::string &name) {
    std::string _name = name;
    if (!CompoundTag_contains(self, &_name, 10)) return NULL;
    return (CompoundTag *) (int) CompoundTag_getShort(self, &_name);
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

// Hack for backwards compat with pre-3.0.0, sane 3.0.0, and this weird new 3.0.0
void misc_add_message(Gui *gui, const char *text) {
    typedef decltype(misc_add_message) *misc_add_message_t;
    static misc_add_message_t original = (misc_add_message_t) dlsym(RTLD_NEXT, "misc_add_message");
    if (!original) {
        // Call the new one :(
        std::string str = text;
        Gui_addMessage(gui, &str);
    } else {
        // Call the old one :D
        original(gui, text);
    }
}
