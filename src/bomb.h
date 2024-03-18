#pragma once

#define BOMB_ITEM_ID 453
#define BOMB_TEXTURE_ID 118
#define BOMB_ID 83
#define BOMB_RENDER_DISPATCHER_ID 22

void make_bomb();
bool really_hit(Throwable *self, HitResult *res);