#include <cmath>

#include <GLES/gl.h>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "init.h"
#include "block_frame.h"

// Tile
static EntityTile *frame = NULL;
static TileEntity *BlockFrame_newTileEntity(UNUSED EntityTile *self);
static int BlockFrame_use(UNUSED EntityTile *self, Level *level, int x, int y, int z, Player *player) {
    FrameTileEntity *frame_te = (FrameTileEntity *) level->getTileEntity(x, y, z);
    ItemInstance *held = player->inventory->getSelected();
    if (!held->isNull() && held->id < 256) {
        frame_te->data.tile = {.count = 1, .id = held->id, .auxiliary = held->auxiliary};
    }
    return 1;
}

static int BlockFrame_getRenderShape(UNUSED EntityTile *self) {
    return /*51 +*/ 0;
}

static bool BlockFrame_isSolidRender(UNUSED EntityTile *self) {
    // Stop it from turning other blocks invisable
    return 0;
}

static int BlockFrame_getRenderLayer(UNUSED EntityTile *self) {
    // Stop weird transparency issues
    return 1;
}

static int BlockFrame_getTexture2(UNUSED EntityTile *self, UNUSED int face, UNUSED int data) {
    return FRAME_TEXTURE;
}

static int BlockFrame_getTexture3(EntityTile *self, LevelSource *_level, int x, int y, int z, int face) {
    if (!_level) return BlockFrame_getTexture2(self, face, 0);
    Level *level = (Level *) _level;
    FrameTileEntity *frame_te = (FrameTileEntity *) mc->level->getTileEntity(x, y, z);
    if (frame_te->data.tile.isNull()) {
        return BlockFrame_getTexture2(self, face, level->getData(x, y, z));
    }
    // Get the custom tile
    Tile *t = Tile::tiles[frame_te->data.tile.id];
    return t->getTexture2(face, frame_te->data.tile.auxiliary);
}

static bool BlockFrame_isCubeShaped(UNUSED EntityTile *self) {
    return false;
}

void make_frame() {
    // Construct
    frame = EntityTile::allocate();
    ALLOC_CHECK(frame);
    int texture = FRAME_TEXTURE;
    frame->constructor(FRAME_TILE_ID, texture, Material::stone);

    // Set VTable
    frame->vtable = dup_vtable(EntityTile_vtable_base);
    ALLOC_CHECK(frame->vtable);
    frame->vtable->newTileEntity = BlockFrame_newTileEntity;
    frame->vtable->use = BlockFrame_use;
    frame->vtable->getRenderShape = BlockFrame_getRenderShape;
    frame->vtable->isSolidRender = BlockFrame_isSolidRender;
    frame->vtable->getRenderLayer = BlockFrame_getRenderLayer;
    frame->vtable->getTexture2 = BlockFrame_getTexture2;
    frame->vtable->getTexture3 = BlockFrame_getTexture3;
    frame->vtable->isCubeShaped = BlockFrame_isCubeShaped;

    // Init
    frame->setShape(0, 0, 0, 1, 0.5f, 1);
    frame->init();
    frame->setDestroyTime(2.0f);
    frame->setExplodeable(5.0f);
    frame->setSoundType(Tile::SOUND_WOOD);
    frame->category = 1;
    frame->setDescriptionId("frame");
}

// Tile entity
static bool FrameTileEntity_shouldSave(UNUSED TileEntity *self) {
    return true;
}

static void FrameTileEntity_load(TileEntity *self, CompoundTag *tag) {
    TileEntity_load->get(false)(self, tag);
    CompoundTag *ctag = CompoundTag_getCompound_but_not(tag, "Tile");
    if (ctag) {
        ItemInstance *i = ItemInstance::fromTag(ctag);
        if (i) {
            ((FrameTileEntity *) self)->data.tile = *i;
            delete i;
        }
    }
}

static bool FrameTileEntity_save(TileEntity *self, CompoundTag *tag) {
    TileEntity_save->get(false)(self, tag);
    CompoundTag *ctag = CompoundTag::allocate();
    ctag->constructor("");
    ((FrameTileEntity *) self)->data.tile.save(ctag);
    std::string tile = "Tile";
    tag->put(tile, (Tag *) ctag);
    return true;
}

CUSTOM_VTABLE(frame_te, TileEntity) {
    vtable->shouldSave = FrameTileEntity_shouldSave;
    vtable->load = FrameTileEntity_load;
    vtable->save = FrameTileEntity_save;
}

static FrameTileEntity *make_frame_tile_entity() {
    FrameTileEntity *frame_te = new FrameTileEntity;
    ALLOC_CHECK(frame_te);
    frame_te->super()->constructor(48);
    frame_te->data.tile = {0, FRAME_TILE_ID, 0};
    frame_te->super()->renderer_id = 11;
    frame_te->super()->vtable = get_frame_te_vtable();
    return frame_te;
}

OVERWRITE_CALLS(TileEntityFactory_createTileEntity, TileEntity *, TileEntityFactory_createTileEntity_injection, (TileEntityFactory_createTileEntity_t original, int id)) {
    if (id == 48) {
        return (TileEntity *) make_frame_tile_entity();
    }
    // Call original
    return original(id);
}

OVERWRITE_CALLS(TileEntity_initTileEntities, void, TileEntity_initTileEntities_injection, (TileEntity_initTileEntities_t original)) {
    // Call original
    original();
    // Add
    std::string str = "Frame";
    TileEntity::setId(48, str);
}

// Tile again
static TileEntity *BlockFrame_newTileEntity(UNUSED EntityTile *self) {
    return (TileEntity *) make_frame_tile_entity();
}