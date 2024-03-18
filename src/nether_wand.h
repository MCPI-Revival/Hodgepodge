#pragma once

#define NETHER_WAND_ID 454
#define NETHER_WAND_TEXTURE 102

int transform_id(int *id, int data, bool *should_heal);
bool transmutate_pedestal(Level *level, Player *player, int x, int y, int z, ItemInstance *item);
void make_nether_wand();
