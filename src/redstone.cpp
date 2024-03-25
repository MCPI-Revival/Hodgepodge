#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "redstone.h"

// Create Item
static Item *redstone = NULL;
static Tile *redstone_wire = NULL;
static Tile *redstone_block = NULL;

int RedStoneOreTile_getResource_injection(UNUSED Tile *t, UNUSED int data, UNUSED Random *random) {
    return REDSTONE_ID;
}

void make_redstone() {
    // Redstone dust
    // TODO: TileItem
    redstone = alloc_Item();
    ALLOC_CHECK(redstone);
    Item_constructor(redstone, REDSTONE_ID - 256);

    std::string name = "redstoneDust";
    redstone->vtable->setDescriptionId(redstone, &name);
    redstone->category = 2;
    redstone->texture = 56;
}

// Redstone tile
static AABB *RedstoneWire_getAABB(UNUSED Tile *self, UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) {
    return NULL;
}
static bool RedstoneWire_isSolidRender(UNUSED Tile *self) {
    return false;
}
static bool RedstoneWire_isCubeShaped(UNUSED Tile *self) {
    return false;
}
static bool RedstoneWire_mayPlace2(UNUSED Tile *self, Level *level, int x, int y, int z) {
    return level->vtable->isSolidRenderTile(level, x, y - 1, z);
}

void make_redstone_wire() {
    // Redstone blocks
    redstone_wire = alloc_Tile();
    ALLOC_CHECK(redstone_wire);
    int texture = 0;
    Tile_constructor(redstone_wire, 55, texture, Material_stone);
    redstone_wire->texture = texture;

    // Set VTable
    redstone_wire->vtable = dup_Tile_vtable(Tile_vtable_base);
    ALLOC_CHECK(redstone_wire->vtable);
    //redstone_wire->vtable->isSignalSource = RedstoneWire_isSignalSource;
    //redstone_wire->vtable->getSignal = RedstoneWire_getSignal;
    //redstone_wire->vtable->getSignal2 = RedstoneWire_getSignal2;
    redstone_wire->vtable->mayPlace2 = RedstoneWire_mayPlace2;
    redstone_wire->vtable->isCubeShaped = RedstoneWire_isCubeShaped;
    redstone_wire->vtable->isSolidRender = RedstoneWire_isSolidRender;
    redstone_wire->vtable->getAABB = RedstoneWire_getAABB;
    redstone_wire->vtable->getResource = RedStoneOreTile_getResource_injection;

    // Init
    Tile_init(redstone_wire);
    redstone_wire->vtable->setDestroyTime(redstone_wire, 0.0f);
    redstone_wire->vtable->setExplodeable(redstone_wire, 0.0f);
    redstone_wire->category = 4;
    std::string name = "redstone";
    redstone_wire->vtable->setDescriptionId(redstone_wire, &name);
    redstone_wire->vtable->setShape(redstone_wire, 0.0f, 0.0f, 0.0f, 1.0f, 0.0625f, 1.0f);
}


// Redstone block
static bool RedstoneBlock_isSignalSource(UNUSED Tile *self) {
    return true;
}

static bool RedstoneBlock_getSignal(UNUSED Tile *self, UNUSED LevelSource *level, UNUSED int x, UNUSED int y, UNUSED int z) {
    return true;
}

static bool RedstoneBlock_getSignal2(UNUSED Tile *self, UNUSED LevelSource *level, UNUSED int x, UNUSED int y, UNUSED int z, UNUSED int direction) {
    return true;
}

void make_redstone_block() {
    // Redstone blocks
    redstone_block = alloc_Tile();
    ALLOC_CHECK(redstone_block);
    int texture = 13+16*7;
    Tile_constructor(redstone_block, 152, texture, Material_stone);
    redstone_block->texture = texture;

    // Set VTable
    redstone_block->vtable = dup_Tile_vtable(Tile_vtable_base);
    ALLOC_CHECK(redstone_block->vtable);
    redstone_block->vtable->isSignalSource = RedstoneBlock_isSignalSource;
    redstone_block->vtable->getSignal = RedstoneBlock_getSignal;
    redstone_block->vtable->getSignal2 = RedstoneBlock_getSignal2;

    // Init
    Tile_init(redstone_block);
    redstone_block->vtable->setDestroyTime(redstone_block, 2.0f);
    redstone_block->vtable->setExplodeable(redstone_block, 10.0f);
    redstone_block->vtable->setSoundType(redstone_block, &Tile_SOUND_STONE);
    redstone_block->category = 4;
    std::string name = "redstone_block";
    redstone_block->vtable->setDescriptionId(redstone_block, &name);
}

bool Level_getSignal_isSolidBlockingTile_injection(Level *level, int x, int y, int z) {
    int id = level->vtable->getTile(level, x, y, z);
    if (id == 152) return false;
    // Call original
    return level->vtable->isSolidBlockingTile(level, x, y, z);
}

__attribute__((constructor)) static void init() {
    // Make the ore drop it
    patch_address((void *) 0x113a38, (void *) RedStoneOreTile_getResource_injection);
    // Fix redstone blocks providing signals
    overwrite_call((void *) 0xa5e8c, (void *) Level_getSignal_isSolidBlockingTile_injection);
}
