#include <GLES/gl.h>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>

#include "api.h"
#include "belt.h"
#include "rendering.h"
#include "oddly_bright_block.h"
#include "block_frame.h"

// The size of a single tile in terrain.png (16 / 256)
constexpr float UV_S = 0.0625;

// Renders the belts
#define RENDER_SQUARE_NO_YZ(x, y, z, xp, yp, zp, u, v, up, vp) \
    Tesselator_vertexUV(t, xp, yp, z,  up, v ); \
    Tesselator_vertexUV(t, xp, y, zp, up, vp); \
    Tesselator_vertexUV(t, x,  y, zp, u,  vp); \
    Tesselator_vertexUV(t, x,  yp, z,  u,  v );

#define RENDER_SQUARE_Y(x, y, z, xp, yp, zp, u, v, up, vp) \
    Tesselator_vertexUV(t, xp, yp, zp, up, v ); \
    Tesselator_vertexUV(t, xp, y,  zp, up, vp); \
    Tesselator_vertexUV(t, x,  y,  z,  u,  vp); \
    Tesselator_vertexUV(t, x,  yp, z,  u,  v );

static void TileRenderer_tesselateBeltInWorld_renderSide(int data, float x, float y, float z, float xp, float zp, int d1, int flipData) {
    Tesselator *t = &Tesselator_instance;
    float u, v, up, vp;
    if (data == d1 || data == flipData) {
        u = (BELT_TOP_TEXTURE & 0xf) << 4; v = (BELT_TOP_TEXTURE & 0xf0);
    } else {
        u = (BELT_SIDE_TEXTURE & 0xf) << 4; v = (BELT_SIDE_TEXTURE & 0xf0);
    }
    u /= 256.f;
    v /= 256.f;
    if (data == flipData) {
        // Flip up/down
        up = u; vp = v + (UV_S / 2);
        u += UV_S;
        v += UV_S;
    } else {
        up = u + UV_S;
        vp = v + UV_S;
        v += (UV_S / 2);
    }
    if (z != zp) {
        // Flip l/r
        float tmp = up;
        up = u;
        u = tmp;
    }
    RENDER_SQUARE_Y(x, y, z, xp, y + 0.5, zp, u, v, up, vp);
}

static bool TileRenderer_tesselateBeltInWorld(TileRenderer *self, Tile *tile, int x, int y, int z, int aux = -1) {
    Tesselator *t = &Tesselator_instance;
    // Set the brightness
    // Bottom
    float brightness = aux == -1 ?
        (float) tile->vtable->getBrightness(tile, (LevelSource *) self->level, x, y, z) * 0.5
        : 0.5;
    brightness *= 256;
    Tesselator_color(t, brightness, brightness, brightness, 0xff);

    float u = (BELT_BOTTOM_TEXTURE & 0xf) << 4, v = (BELT_BOTTOM_TEXTURE & 0xf0);
    u /= 256.f;
    v /= 256.f;
    RENDER_SQUARE_NO_YZ(x, y, z, x + 1, y, z + 1, u, v, u + UV_S, v + UV_S);
    // Top
    u = (BELT_TOP_TEXTURE & 0xf) << 4; v = (BELT_TOP_TEXTURE & 0xf0);
    u /= 256.f;
    v /= 256.f;
    // Cursedness for texture rotation
    float us[4], vs[4];
    int data = aux == -1 ? self->level->vtable->getData(self->level, x, y, z) : aux;
    if (data == 2) {
        us[0] = us[1] = u + UV_S;
        us[2] = us[3] = u;
        vs[1] = vs[2] = v + UV_S;
        vs[0] = vs[3] = v;
    } else if (data == 3) {
        us[0] = us[1] = u;
        us[2] = us[3] = u + UV_S;
        vs[1] = vs[2] = v;
        vs[0] = vs[3] = v + UV_S;
    } else if (data == 4) {
        us[0] = us[3] = u + UV_S;
        us[2] = us[1] = u;
        vs[3] = vs[2] = v + UV_S;
        vs[0] = vs[1] = v;
    } else {
        us[0] = us[3] = u;
        us[2] = us[1] = u + UV_S;
        vs[3] = vs[2] = v;
        vs[0] = vs[1] = v + UV_S;
    }
    Tesselator_vertexUV(t, x + 1, y + 0.5, z + 1, us[0], vs[0]);
    Tesselator_vertexUV(t, x + 1, y + 0.5, z,     us[1], vs[1]);
    Tesselator_vertexUV(t, x,     y + 0.5, z,     us[2], vs[2]);
    Tesselator_vertexUV(t, x,     y + 0.5, z + 1, us[3], vs[3]);
    // Sides
    TileRenderer_tesselateBeltInWorld_renderSide(data, x, y, z, x + 1, z, 2, 3);
    TileRenderer_tesselateBeltInWorld_renderSide(data, x + 1, y, z + 1, x, z + 1, 3, 2);
    TileRenderer_tesselateBeltInWorld_renderSide(data, x, y, z + 1, x, z, 4, 5);
    TileRenderer_tesselateBeltInWorld_renderSide(data, x + 1, y, z, x + 1, z + 1, 5, 4);
    return true;
}

// Render the OBB
static constexpr int COLORS[] = {
    0xFFFFFF,
    0x1e1b1b, 0xb3312c, 0x3b511a, 0x51301a, 0x253192, 0x7b2fbe, 0x287697, 0x287697,
    0x434343, 0xd88198, 0x41cd34, 0xdecf2a, 0x6689d3, 0xc354cd, 0xeb8844, 0xf0f0f0
};
static bool TileRenderer_tesselateOddlyBrightBlockInWorld(TileRenderer *self, UNUSED Tile *tile, int x, int y, int z) {
    Tesselator *t = &Tesselator_instance;
    int data = self->level->vtable->getData(self->level, x, y, z);
    Tesselator_colorABGR(t, 0xFF000000 | COLORS[(~data & 0xf)]);
    // Top and bottom
    RENDER_SQUARE_NO_YZ(x, y, z, x + 1, y, z + 1, 0, 0, 0, 0);
    RENDER_SQUARE_NO_YZ(x, y + 1, z + 1, x + 1, y + 1, z, 0, 0, 0, 0);
    // X sides
    RENDER_SQUARE_Y(x, y, z, x + 1, y + 1, z, 0, 0, 0, 0);
    RENDER_SQUARE_Y(x + 1, y, z + 1, x, y + 1, z + 1, 0, 0, 0, 0);
    // Z sides
    RENDER_SQUARE_Y(x, y, z + 1, x, y + 1, z, 0, 0, 0, 0);
    RENDER_SQUARE_Y(x + 1, y, z, x + 1, y + 1, z + 1, 0, 0, 0, 0);
    return true;
}

// Render of Pedestal
static bool TileRenderer_tesselatePedestalInWorld(TileRenderer *self, Tile *tile, int x, int y, int z, bool gui = false) {
    int old_texture = tile->texture;

    // Shaft
    tile->texture = 213;
    tile->vtable->setShape(tile,
        0.2, 0.0, 0.2,
        0.8, 0.8, 0.8
    );
    if (gui) {
        TileRenderer_renderGuiTile(self, tile, 0);
    } else {
        TileRenderer_tesselateBlockInWorld(self, tile, x, y, z);
    }

    // Top layer 1
    tile->texture = 212;
    tile->vtable->setShape(tile,
        0.05, 0.8, 0.05,
        0.95, 0.9, 0.95
    );
    if (gui) {
        TileRenderer_renderGuiTile(self, tile, 0);
    } else {
        TileRenderer_tesselateBlockInWorld(self, tile, x, y, z);
    }

    // Top layer 2
    tile->vtable->setShape(tile,
        0.0, 0.90, 0.0,
        1.0, 1.0,  1.0
    );
    if (gui) {
        TileRenderer_renderGuiTile(self, tile, 0);
    } else {
        TileRenderer_tesselateBlockInWorld(self, tile, x, y, z);
    }

    // Done
    tile->vtable->setShape(tile, 0, 0, 0, 1, 1, 1);
    tile->texture = old_texture;
    return true;
}

// Inject
static bool TileRenderer_tesselateInWorld_injection(TileRenderer *self, Tile *tile, int x, int y, int z) {
    int shape = tile->vtable->getRenderShape(tile);
    if (shape == 47) {
        // Belts
        return TileRenderer_tesselateBeltInWorld(self, tile, x, y, z);
    } else if (shape == 48) {
        // Oddly bright block
        return TileRenderer_tesselateOddlyBrightBlockInWorld(self, tile, x, y, z);
    } else if (shape == 49) {
        // Pedestal
        return TileRenderer_tesselatePedestalInWorld(self, tile, x, y, z);
    }
    return TileRenderer_tesselateInWorld(self, tile, x, y, z);
}
static void TileRenderer_renderGuiTile_injection(TileRenderer *self, Tile *tile, int aux) {
    // This renders in gui slots
    int shape = tile->vtable->getRenderShape(tile);
    if (shape == 47) {
        // Belts
        Tesselator_addOffset(&Tesselator_instance, -0.5, -0.5, -0.5);
        Tesselator_begin(&Tesselator_instance, 7);
        TileRenderer_tesselateBeltInWorld(self, tile, -0.5f, -0.5f, -0.5f, 3);
        Tesselator_draw(&Tesselator_instance);
        Tesselator_addOffset(&Tesselator_instance, 0.5, 0.5, 0.5);
    } else if (shape == 49) {
        // Pedestal
        TileRenderer_tesselatePedestalInWorld(self, tile, -0.5f, -0.5f, -0.5f, true);
    } else {
        TileRenderer_renderGuiTile(self, tile, aux);
    }
}
static void TileRenderer_renderTile_injection(TileRenderer *self, Tile *tile, int aux) {
    // This renders for anything ItemInHandRenderer::renderItem uses (which includes dropped items afaik)
    int shape = tile->vtable->getRenderShape(tile);
    if (shape == 47) {
        // Belts
        Tesselator_addOffset(&Tesselator_instance, -0.5, -0.5, -0.5);
        Tesselator_begin(&Tesselator_instance, 7);
        TileRenderer_tesselateBeltInWorld(self, tile, 0, 0, 0, 3);
        Tesselator_draw(&Tesselator_instance);
        Tesselator_addOffset(&Tesselator_instance, 0.5, 0.5, 0.5);
    } else if (shape == 49) {
        // Pedestal
        TileRenderer_tesselatePedestalInWorld(self, tile, -0.5f, -0.5f, -0.5f, true);
    } else {
        TileRenderer_renderGuiTile(self, tile, aux);
    }
}

OVERWRITE_CALLS(TileRenderer_canRender, bool, TileRenderer_canRender_injection, (int shape)) {
    return TileRenderer_canRender(shape) || shape == 47 || shape == 49;
}

// Belt dynamic texture
static void BeltTexture_tick(DynamicTexture *self) {
    static uint belt_colors[] = {0xFF8f8f8f, 0xFF8f8f8f, 0xFF595959, 0xFF595959};
    static constexpr int side_color = 0xFF000000;
    static int offset = 8;
    offset -= 1;
    if (offset <= 0) offset = 8;

    for (int y = 0; y < 16; y++) {
        // Sides
        ((uint *) self->pixels)[0 + y * 16] = side_color;
        ((uint *) self->pixels)[15 + y * 16] = side_color;
        // Inner part
        for (int x = 1; x < 15; x++) {
            ((uint *) self->pixels)[x + y * 16] = belt_colors[((y + offset) % 4)];
        }
    }
}

CUSTOM_VTABLE(belt_texture, DynamicTexture) {
    vtable->tick = BeltTexture_tick;
}

static DynamicTexture *create_belt_texture() {
    DynamicTexture *belt_texture = new DynamicTexture;
    ALLOC_CHECK(belt_texture);
    DynamicTexture_constructor(belt_texture, BELT_TOP_TEXTURE);
    belt_texture->vtable = get_belt_texture_vtable();
    return belt_texture;
}

HOOK_FROM_CALL(0x170b4, void, Textures_addDynamicTexture, (Textures *self, DynamicTexture *tex)) {
    Textures_addDynamicTexture(self, tex);

    static bool ran = false;
    if (!ran) {
        Textures_addDynamicTexture(self, create_belt_texture());
        ran = true;
    }
}

// Fix ItemRenderer_renderGuiItem_two
static float w = 16, h = 16;
void ItemRenderer_renderGuiItem2(Font *font, Textures *textures, ItemInstance *item_instance, float x, float y, float w_ /*= 16*/, float h_ /*= 16*/, bool param_5 /*=true*/) {
    w = w_; h = h_;
    ItemRenderer_renderGuiItem_two(font, textures, item_instance, x, y, w, h, param_5);
    w = h = 16;
}

// Fix items
OVERWRITE_CALL(0x63b74, void, ItemRenderer_blit_injection, (float x, float y, float texture_x, float texture_y, UNUSED float _w, UNUSED float _h)) {
    // Center them
    //x += (16 - w) / 2;
    //y += (16 - h) / 2;
    // Decompiled version of ItemRenderer::blit, that fixes textures on smaller scales
    float od256 = 1.f / 256.f;
    float v = (texture_y + 16) * od256;
    float u = (texture_x + 16) * od256;
    Tesselator *t = &Tesselator_instance;
    Tesselator_begin(t, 7);
    Tesselator_vertexUV(t, x,     y + h, 0.0, texture_x * od256, v);
    Tesselator_vertexUV(t, x + w, y + h, 0.0, u,                 v);
    Tesselator_vertexUV(t, x + w, y,     0.0, u,                 texture_y * od256);
    Tesselator_vertexUV(t, x,     y,     0.0, texture_x * od256, texture_y * od256);
    Tesselator_draw(t);
}

// Fix tiles
OVERWRITE_CALL(0x63a90, void, glScalef_injection, (UNUSED float x, UNUSED float y, UNUSED float z)) {
    glScalef(w / 16.f, h / 16.f, w / 16.f);
}

OVERWRITE_CALL(0x63a38, void, glTranslatef_injection, (float x, float y, float z)) {
    // TODO: Fix
    // AAAAAAAAAAAAAAAAAAAAAAAAAHSRTGRFWEfwejfwkefbJKBFNKWEBJWFWKEBJKWEBFWHJEBFWHJBFHJWEB
    // I HATE GRAPHIC CODE I HATE GRAPHICS CODE I HATE GRAPHICS CODE
    glTranslatef(x / (w / 16.f), y / (h / 16.f), z);
}

__attribute__((constructor)) static void init() {
    // Tesselatation
    overwrite_calls((void *) TileRenderer_tesselateInWorld, (void *) TileRenderer_tesselateInWorld_injection);
    overwrite_calls((void *) TileRenderer_renderGuiTile, (void *) TileRenderer_renderGuiTile_injection);
    overwrite_calls((void *) TileRenderer_renderTile, (void *) TileRenderer_renderTile_injection);

    // Fix aux in hand bug
    uchar cmp_r7_patch[] = {0x07, 0x00, 0x57, 0xe1}; // "cmp r7,r7"
    patch((void *) 0x4b938, cmp_r7_patch);
    uchar moveq_r3_true_patch[] = {0x01, 0x30, 0xa0, 0x03}; // "moveq r3,#0x1"
    patch((void *) 0x4b93c, moveq_r3_true_patch);
}
