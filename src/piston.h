#pragma once

extern Tile *piston_base;
extern Tile *sticky_piston_base;
extern EntityTile *piston_moving;
extern Tile *piston_head;

#define PISTON_SIDE_TEXTURE (12+16*6)
#define PISTON_HEAD_TEXTURE (11+16*6)
#define PISTON_HEAD_TEXTURE_STICKY (10+16*6)
#define PISTON_BACK_TEXTURE (13+16*6)
#define PISTON_HEAD_TEXTURE_EXTENDED (14+16*6)

// Switch for quasiconnectivity
#define P_USE_QC 1
#define P_SPEED 0.5f
#define P_RANGE 16
void make_pistons();