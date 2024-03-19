#pragma once

#define FRAME_TILE_ID 173
#define FRAME_TEXTURE 187

void make_frame();

struct FrameTileEntity : TileEntity {
    ItemInstance tile;
};
