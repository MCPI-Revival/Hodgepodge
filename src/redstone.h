// These are the item ids
#define REDSTONE_ID 331
#define REPEATER_ID 356
#define REDSTONE_TEXTURE_S2 165
#define REDSTONE_TEXTURE_S4 164

bool canWireConnectTo(LevelSource *level, int x, int y, int z, int side);
void make_redstone_wire();
void make_redstone_blocks();
void make_lamp(int id, bool active);
extern bool repeater_rendering_torches;
