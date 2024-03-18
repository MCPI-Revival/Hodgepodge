#pragma once

#define PEDESTAL_ID 172

void make_pedestal();

struct PedestalTileEntity : TileEntity {
    ItemInstance item;
};
