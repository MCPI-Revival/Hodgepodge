#pragma once

#define PEDESTAL_ID 172

void make_pedestal();

EXTEND_STRUCT(PedestalTileEntity, TileEntity, struct {
    ItemInstance item;
});
