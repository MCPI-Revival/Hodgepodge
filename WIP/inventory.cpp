#include <GLES/gl.h>
#include <SDL/SDL.h>

//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>
#include <media-layer/core.h>

#include "api.h"

struct InventoryScreen : Screen {
    int xSize = 176;
    int ySize = 166;
    Container *inventory = NULL;
};

bool hotbar_needs_update = false;
int hotbar_offset = 0;
void set_hotbar(Inventory *inv);

typedef void (*Font_drawShadow_t)(Font *font, const char *str, float x, float y, int color);
Font_drawShadow_t Font_drawShadow = (Font_drawShadow_t) 0x25150;
typedef void (*ArmorScreen_renderPlayer_t)(Screen *_self, float x, float y);
ArmorScreen_renderPlayer_t ArmorScreen_renderPlayer = (ArmorScreen_renderPlayer_t) 0x29cd0;

static void renderItem(Minecraft *mc, ItemInstance *item, int x, int y, float scale) {
    // Item
    ItemRenderer_renderGuiItem_one(mc->font, mc->textures, item, x, y, 1);
    // Damage
    ItemRenderer_renderGuiItemDecorations(item, x, y);
    // Count
    glPushMatrix();
    glScalef(scale, scale, 1);
    if (item->count != 1) {
        std::string count = item->count < 100 ? std::to_string(item->count) : "99+";
        Font_drawShadow(mc->font, count.c_str(), x / scale, y / scale, (item->count > 0) ? 0xFFF0F0F0 : 0xFFFF5555);
    }
    glPopMatrix();
}

static void InventoryScreen_render(Screen *_self, UNUSED int mx, UNUSED int my, UNUSED float param_1) {
    InventoryScreen *self = (InventoryScreen *) _self;
    //if (!self->minecraft || !self->minecraft->player || !self->minecraft->player->inventory) return;

    const float scale = Gui_InvGuiScale * 2;
    const int size_x = 255, size_y = 166;
    const int cx = self->width / (2 * scale), cy = self->height / (2 * scale);
    const int startx = std::max(0, cx - (size_x / 2)), starty = std::max(0, cy - (size_y / 2));

    // BG
    GuiComponent_fillGradient((GuiComponent *) self, 0, 0, self->width, starty + 88 * scale, 0xc0101010, 0xd0101010);

    // UI
    glPushMatrix();
    glScalef(scale, scale, 1);
    static std::string name = "gui/inventory.png";
    Textures_loadAndBindTexture(self->minecraft->textures, &name);
    GuiComponent_blit(
        (GuiComponent *) self,
        startx, starty,
        0, 0,
        size_x, size_y,
        0, 0
    );
    // Hotbar arrow
    GuiComponent_blit(
        (GuiComponent *) self,
        startx + 73, starty + 87 + (hotbar_offset * 18),
        size_x + 33, size_y,
        16, 16,
        0, 0
    );

    // Render player
    glPushMatrix();
    glScalef(scale, scale, 1);
    ArmorScreen_renderPlayer(_self, startx/scale + (8 + 8) / (scale / 2), starty/scale + (88 - 11) / (scale / 2));
    glPopMatrix();

    // Render items
    Inventory *inventory = self->minecraft->player->inventory;
    int xo = 88 + startx, yo = 88 + starty;
    const int offset = 18;
    int x = 0, y = 0;
    int size = std::min(9 * 5, inventory->vtable->getContainerSize(inventory));
    for (int i = 9; i < size; i++) {
        ItemInstance *item = inventory->vtable->getItem(inventory, i);
        if (item) {
            renderItem(self->minecraft, item, xo + x * offset, yo + y * offset, scale);
        }
        x++;
        if (x == 9) {
            x = 0;
            y++;
        }
    }
    // Render armor
    xo = 58 + startx;
    y = x = 0;
    for (int i = 0; i < 4; i++) {
        ItemInstance *item = LocalPlayer_getArmor(self->minecraft->player, i);
        if (item) {
            renderItem(self->minecraft, item, xo + x * offset, yo + i * offset, scale);
        }
    }
    glPopMatrix();
}

Screen_vtable *get_inventory_screen_vtable() {
    static Screen_vtable *vtable = NULL;
    if (!vtable) {
        vtable = dup_Screen_vtable(Screen_vtable_base);
        ALLOC_CHECK(vtable);
        vtable->render = InventoryScreen_render;
    }
    return vtable;
}

HOOK_SINGLE_FROM_CALL(0x293a0, Touch_IngameBlockSelectionScreen *, Touch_IngameBlockSelectionScreen_constructor, (Touch_IngameBlockSelectionScreen *self)) {
    Screen_constructor((Screen *) self);
    self->vtable = (Touch_IngameBlockSelectionScreen_vtable *) get_inventory_screen_vtable();
    return self;
}

static void *new_injection() {
    return (void *) new InventoryScreen;
}

__attribute__((constructor)) static void init() {
    overwrite_call((void *) 0x2943c, (void *) new_injection);
    overwrite_call((void *) 0x29398, (void *) new_injection);
    overwrite_call((void *) 0x29444, (void *) Touch_IngameBlockSelectionScreen_constructor_injection);
}

// Fix linked slots
void set_hotbar(Inventory *inv) {
    for (int i = 0; i < 9; i++) {
        inv->linked_slots[i] = i + 9 * (hotbar_offset + 1);
    }
}

// TODO: Clear inventory *without* default once item bar exists
HOOK_FROM_CALL(0x8e7dc, void, Inventory_setupDefault, (Inventory *self)) {
    Inventory_setupDefault_original(self);
    set_hotbar(self);
}
