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
    FrameTileEntity *frame_te = (FrameTileEntity *) Level_getTileEntity(level, x, y, z);
    ItemInstance *held = Inventory_getSelected(player->inventory);
    if (!ItemInstance_isNull(held) && held->id < 256) {
        frame_te->tile = {.count = 1, .id = held->id, .auxiliary = held->auxiliary};
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
    FrameTileEntity *frame_te = (FrameTileEntity *) Level_getTileEntity(mc->level, x, y, z);
    if (ItemInstance_isNull(&frame_te->tile)) {
        return BlockFrame_getTexture2(self, face, level->vtable->getData(level, x, y, z));
    }
    // Get the custom tile
    Tile *t = Tile_tiles[frame_te->tile.id];
    return t->vtable->getTexture2(t, face, frame_te->tile.auxiliary);
}

static bool BlockFrame_isCubeShaped(UNUSED EntityTile *self) {
    return false;
}

void make_frame() {
    // Construct
    frame = alloc_EntityTile();
    ALLOC_CHECK(frame);
    int texture = FRAME_TEXTURE;
    Tile_constructor((Tile *) frame, FRAME_TILE_ID, texture, Material_stone);
    Tile_isEntityTile[FRAME_TILE_ID] = true;
    frame->texture = texture;

    // Set VTable
    frame->vtable = dup_EntityTile_vtable(EntityTile_vtable_base);
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
    Tile_setShape_non_virtual((Tile *) frame, 0, 0, 0, 1, 0.5f, 1);
    EntityTile_init(frame);
    frame->vtable->setDestroyTime(frame, 2.0f);
    frame->vtable->setExplodeable(frame, 5.0f);
    frame->vtable->setSoundType(frame, &Tile_SOUND_WOOD);
    frame->category = 1;
    std::string name = "frame";
    frame->vtable->setDescriptionId(frame, &name);
}

// Tile entity
static bool FrameTileEntity_shouldSave(UNUSED TileEntity *self) {
    return true;
}

static void FrameTileEntity_load(TileEntity *self, CompoundTag *tag) {
    TileEntity_load_non_virtual(self, tag);
    CompoundTag *ctag = CompoundTag_getCompound_but_not(tag, "Tile");
    if (ctag) {
        ItemInstance *i = ItemInstance_fromTag(ctag);
        if (i) {
            ((FrameTileEntity *) self)->tile = *i;
            delete i;
        }
    }
}

static bool FrameTileEntity_save(TileEntity *self, CompoundTag *tag) {
    TileEntity_save_non_virtual(self, tag);
    CompoundTag *ctag = alloc_CompoundTag();
    CompoundTag_constructor(ctag, "");
    ItemInstance_save(&((FrameTileEntity *) self)->tile, ctag);
    std::string tile = "Tile";
    CompoundTag_put(tag, &tile, (Tag *) ctag);
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
    TileEntity_constructor(frame_te, 48);
    frame_te->tile = {0, FRAME_TILE_ID, 0};
    frame_te->renderer_id = 11;
    frame_te->vtable = get_frame_te_vtable();
    return frame_te;
}

HOOK_FROM_CALL(0xd2544, TileEntity *, TileEntityFactory_createTileEntity, (int id)) {
    if (id == 48) {
        return make_frame_tile_entity();
    }
    // Call original
    return TileEntityFactory_createTileEntity_original(id);
}

HOOK_FROM_CALL(0x149b0, void, TileEntity_initTileEntities, ()) {
    // Call original
    TileEntity_initTileEntities_original();
    // Add
    std::string str = "Frame";
    TileEntity_setId(48, &str);
}

// Tile again
static TileEntity *BlockFrame_newTileEntity(UNUSED EntityTile *self) {
    return make_frame_tile_entity();
}