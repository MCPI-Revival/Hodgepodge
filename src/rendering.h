#pragma once

// The size of a single tile in terrain.png (16 / 256)
#define UV_S 0.0625

void ItemRenderer_renderGuiItem2(Font *font, Textures *textures, ItemInstance *item_instance, float x, float y, float w = 16, float h = 16, bool param_5 = true);
bool TileRenderer_tesselateBlockInWorldWithTextureRotation(
    Tile *tile, float x, float y, float z, int data, Level *level,
    float cutoff = 0, float ncutoff = 0, bool shift = false,
    float v_size = UV_S
);
