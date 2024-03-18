#pragma once

#define FRAME_TILE_ID 173

void make_frame();

struct FrameTileEntity : TileEntity {
    ItemInstance tile;
};
