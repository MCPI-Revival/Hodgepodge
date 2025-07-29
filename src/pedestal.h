#pragma once

#define PEDESTAL_ID 172

void make_pedestal();

struct PedestalTileEntity;
ItemInstance &get_pedestal_item(PedestalTileEntity *pedestal);