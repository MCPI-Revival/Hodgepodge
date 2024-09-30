#pragma once

#define FRAME_TILE_ID 173
#define FRAME_TEXTURE 187

void make_frame();

EXTEND_STRUCT(FrameTileEntity, TileEntity, struct {
    ItemInstance tile;
});
