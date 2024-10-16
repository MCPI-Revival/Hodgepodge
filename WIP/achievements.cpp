#include <cmath>

#include <GLES/gl.h>
#include <SDL/SDL.h>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <media-layer/core.h>

#include "api.h"
#include "achievements.h"
#include "rendering.h"
//#include "inventory.h"

struct achievement_t {
    std::string name = "Test";
    std::string desc = "No way, it works!";
    int item_id = 1;
    int x = 20, y = 20;
    bool completed = false;
    bool hidden = false, force_hidden = false;
    std::vector<size_t> deps = {};
    // TODO
    std::vector<ItemInstance> rewards = {};
    int section = 0;
};

static std::vector<achievement_t> achievements = {
    {"Taking Inventory", "Press 'E' to open your inventory",         340, .x = 20,  .y = 20,  .completed = true},
    {"Getting Wood", "Attack a tree until a block of wood pops out", 81,  .x = 20,  .y = 60,  .deps = {0}}, // 17
    {"Benchmarking", "Craft a workbench with four blocks of planks", 58,  .x = 20,  .y = 100, .deps = {1}},
    {"Time to Mine!", "Use planks and sticks to make a pickaxe",     270, .x = 60,  .y = 20,  .deps = {2}},
    {"Time to Strike!", "Use planks and sticks to make a sword",     268, .x = 60,  .y = 60,  .deps = {2}},
    {"Time to Farm!", "Use planks and sticks to make a hoe",         290, .x = 60,  .y = 100, .deps = {2}},
    {"Bake Bread", "Turn wheat into bread",                          297, .x = 20,  .y = 140, .deps = {5}},
    {"Have your cake...", "...and eat it too!",                      92,  .x = 60,  .y = 140, .deps = {5}},
};

struct AchievementScreen : Screen {
    int section = 0;
};

struct Vec2 {
    float x, y;
    inline Vec2(float x, float y) : x(x), y(y) {}
    inline Vec2(achievement_t &a) : x(a.x + 8), y(a.y + 8) {}
    inline Vec2 operator+(Vec2 &other) {
        return Vec2(x + other.x, y + other.y);
    }
    inline Vec2 operator-(Vec2 &other) {
        return Vec2(x - other.x, y - other.y);
    }
    // I can't believe I need to spell this out
    inline Vec2 operator+=(Vec2 &other) {*this = *this + other; return *this;}
    inline Vec2 operator-=(Vec2 &other) {*this = *this - other; return *this;}
};

constexpr float UV_S = 0.0625;
static float scale = 1;
static Vec2 offset = {0, 0};
static void AchievementScreen_render(Screen *self, UNUSED int x, UNUSED int y, UNUSED float param_1) {
    // BG
    GuiComponent_fillGradient((GuiComponent *) self, 0, 0, self->width, self->height, 0xc0101010, 0xd0101010);
    // Say hello!
    //std::string msg = "haiii :3";
    //GuiComponent_drawCenteredString((GuiComponent *) self, self->minecraft->font, &msg, self->width / 2, self->height / 2, 0xc0FFFFFF);
    // Achivements
    int mx = Mouse_getX() * Gui_InvGuiScale, my = Mouse_getY() * Gui_InvGuiScale;
    achievement_t *hovered = NULL;
    // Achievements are rendered backwards to correctly draw dep lines
    for (int at = achievements.size() - 1; at >= 0; at--) {
        auto &achievement = achievements.at(at);
        if (achievement.section != ((AchievementScreen *) self)->section) continue;
        // Don't show hidden until they are completed
        if (achievement.hidden && !achievement.completed) continue;
        // Dep lines
        bool can_be_rendered = true;
        for (int dep : achievement.deps) {
            achievement_t &parent = achievements.at(dep);
            // Don't show children of hidden achievements until they are completed
            if ((parent.hidden && !parent.completed) || parent.force_hidden) {
                achievement.force_hidden = true;
                can_be_rendered = false;
                continue;
            }
            // Stupid edge case
            if (parent.x == achievement.x && parent.y == achievement.y) {
                continue;
            }
            // See https://www.desmos.com/calculator/n78wzgstfj for a visualization
            // Get the middle points and angle
            Vec2 p1 = Vec2(achievement), p2 = Vec2(parent);
            float a = atan2(p2.y - p1.y, p2.x - p1.x) + M_PI/2;
            // Get the edge points
            constexpr float thickness = 2;
            Vec2 p1_1{thickness * float(cos(a)) + p1.x, thickness * float(sin(a)) + p1.y};
            Vec2 p1_2 = (p1 - p1_1) + p1;
            Vec2 p2_1 = (p1 - p1_1) + p2 + offset;
            Vec2 p2_2 = (p1 - p1_2) + p2 + offset;
            p1_1 += offset;
            p1_2 += offset;
            // Draw
            Tesselator *t = &Tesselator_instance;
            glEnable(GL_BLEND);
            glDisable(GL_TEXTURE_2D);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            Tesselator_begin(t, 7);
            // Green line for possible, gray line otherwise
            if (parent.completed) {
                Tesselator_colorABGR(t, 0xFF68BB68);
            } else {
                Tesselator_colorABGR(t, 0xFF686868);
            }
            Tesselator_vertexUV(t, p2_2.x / scale, p2_2.y / scale, 0, 1, 0);
            Tesselator_vertexUV(t, p2_1.x / scale, p2_1.y / scale, 0, 1, 1);
            Tesselator_vertexUV(t, p1_2.x / scale, p1_2.y / scale, 0, 0, 1);
            Tesselator_vertexUV(t, p1_1.x / scale, p1_1.y / scale, 0, 0, 0);
            Tesselator_draw(t);
            glEnable(GL_TEXTURE_2D);
            glDisable(GL_BLEND);
        }
        if (!can_be_rendered) continue;
        // Item BG
        std::string s = "gui/gui.png";
        Textures_loadAndBindTexture(self->minecraft->textures, &s);
        GuiComponent_blit(
            (GuiComponent *) self,
            (achievement.x - 3 + offset.x) / scale, (achievement.y - 3 + offset.y) / scale,
            (14 - achievement.completed) * 16, 0,
            22 / scale, 22 / scale,
            16, 16
        );
        // Item icon
        ItemInstance item = {.count = 1, .id = achievement.item_id, .auxiliary = 0};
        ItemRenderer_renderGuiItem2(self->minecraft->font, self->minecraft->textures, &item, (achievement.x - 3 + offset.x) / scale, (achievement.y - 3 + offset.y) / scale, 16 / scale, 16 / scale);
        // Check if hovered
        if (
            ((achievement.x + offset.x) / scale <= mx && mx <= (achievement.x + 16 + offset.x) / scale) &&
            ((achievement.y + offset.y) / scale <= my && my <= (achievement.y + 16 + offset.y) / scale)
        ) {
            hovered = &achievement;
        }
    }
    // Render hovered
    if (hovered) {
        int width = Font_width(self->minecraft->font, &hovered->name);
        Vec2 hovPos{hovered->x + 18.f, hovered->y + 8.f};
        hovPos += offset;
        width = std::max(width, Font_width(self->minecraft->font, &hovered->desc));
        GuiComponent_fill((GuiComponent *) self, (hovPos.x - 2) / scale, (hovPos.y - 2) / scale, (hovPos.x + width + 2) / scale, (hovPos.y + 18) / scale, 0xFF505050);
        GuiComponent_drawString((GuiComponent *) self, self->minecraft->font, &hovered->name, hovPos.x, hovPos.y, 0xFFDDDDDD);
        GuiComponent_drawString((GuiComponent *) self, self->minecraft->font, &hovered->desc, hovPos.x, hovPos.y + 8, 0xFFD0D0D0);
    }
}

CUSTOM_VTABLE(achievement_screen, Screen) {
    // Rendering
    vtable->render = AchievementScreen_render;
}

Screen *create_achievement_screen() {
    // Construct
    AchievementScreen *screen = new AchievementScreen;
    ALLOC_CHECK(screen);
    Screen_constructor((Screen *) screen);

    // Set VTable
    screen->vtable = get_achievement_screen_vtable();

    // Reset
    scale = 1;

    // Return
    return (Screen *) screen;
}

#define SCROLL_SENSITIVITY 0.1f
bool achievement_wants_open = false;
HOOK(SDL_PollEvent, int, (SDL_Event *event)) {
    int ret = real_SDL_PollEvent()(event);
    if (ret == 1 && event != NULL) {
        if (event->type == SDL_USEREVENT && event->user.data1 == SDL_PRESSED && event->user.code == USER_EVENT_REAL_KEY) {
            // Opening achivements screen
            if (event->user.data2 == 78) {
                achievement_wants_open = true;
            } /*else if (event->user.data2 == 82) {
                // Hotbar offsetting
                hotbar_offset += 1;
                hotbar_offset %= 4;
                hotbar_needs_update = true;
            }*/
            // Scrolling with arrows
            if (event->user.data2 == 265) {
                scale = std::min(scale + SCROLL_SENSITIVITY, 4.0f);
            } else if (event->user.data2 == 264) {
                scale = std::max(scale - SCROLL_SENSITIVITY, 0.6f);
            }
        } else if (event->type == SDL_MOUSEBUTTONDOWN) {
            // Scrolling with scroll wheel
            if (event->button.button == SDL_BUTTON_WHEELUP) {
                scale = std::min(scale + SCROLL_SENSITIVITY, 4.0f);
            } else if (event->button.button == SDL_BUTTON_WHEELDOWN) {
                scale = std::max(scale - SCROLL_SENSITIVITY, 0.6f);
            }
        }
    }
    return ret;
}
