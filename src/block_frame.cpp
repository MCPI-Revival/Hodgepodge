#include <cmath>

#include <GLES/gl.h>

//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "init.h"
#include "block_frame.h"

// Tile entity
struct FrameTileEntity final : CustomTileEntity {
    ItemInstance tile;

    FrameTileEntity(int id) : CustomTileEntity(id) {}

    bool shouldSave() override {
        return true;
    }

    void load(CompoundTag *tag) override {
        TileEntity_load->get(false)(self, tag);
        CompoundTag *ctag = CompoundTag_getCompound_but_not(tag, "Tile");
        if (ctag) {
            ItemInstance *i = ItemInstance::fromTag(ctag);
            if (i) {
                this->tile = *i;
                delete i;
            }
        }
    }

    bool save(CompoundTag *tag) override {
        TileEntity_save->get(false)(self, tag);
        CompoundTag *ctag = CompoundTag::allocate();
        ctag->constructor("");
        tile.save(ctag);
        std::string tile = "Tile";
        tag->put(tile, (Tag *) ctag);
        return true;
    }
};

static FrameTileEntity *make_frame_tile_entity() {
    FrameTileEntity *frame_te = new FrameTileEntity(48);
    frame_te->tile = {0, FRAME_TILE_ID, 0};
    frame_te->self->renderer_id = 11;
    return frame_te;
}

OVERWRITE_CALLS(
    TileEntityFactory_createTileEntity, TileEntity *, TileEntityFactory_createTileEntity_injection, (TileEntityFactory_createTileEntity_t original, int id)
) {
    if (id == 48) {
        return (TileEntity *) make_frame_tile_entity()->self;
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

// Tile
static EntityTile *frame = NULL;
void make_frame();
struct BlockFrame final : CustomEntityTile {
    BlockFrame(const int id, const int texture, const Material *material): CustomEntityTile(id, texture, material) {}

    bool use(Level *level, int x, int y, int z, Player *player) override {
        FrameTileEntity *frame_te = (FrameTileEntity *) custom_get<CustomTileEntity>(level->getTileEntity(x, y, z));
        ItemInstance *held = player->inventory->getSelected();
        if (!held->isNull() && held->id < 256) {
            frame_te->tile = {.count = 1, .id = held->id, .auxiliary = held->auxiliary};
        }
        return true;
    }

    int getRenderShape() override {
        return /*51 +*/ 0;
    }

    bool isSolidRender() override {
        // Stop it from turning other blocks invisable
        return 0;
    }

    int getRenderLayer() override {
        // Stop weird transparency issues
        return 1;
    }

    int getTexture2(UNUSED int face, UNUSED int data) override {
        return FRAME_TEXTURE;
    }

    int getTexture3(LevelSource *levelsrc, int x, int y, int z, int face) override {
        if (!levelsrc) return getTexture2(face, 0);
        Level *level = (Level *) levelsrc;
        FrameTileEntity *frame_te = (FrameTileEntity *) custom_get<CustomTileEntity>(mc->level->getTileEntity(x, y, z));
        if (frame_te->tile.isNull()) {
            return getTexture2(face, level->getData(x, y, z));
        }
        // Get the custom tile
        Tile *t = Tile::tiles[frame_te->tile.id];
        return t->getTexture2(face, frame_te->tile.auxiliary);
    }

    bool isCubeShaped() override {
        return false;
    }

    TileEntity *newTileEntity() {
       return (TileEntity *) make_frame_tile_entity();
   }
};

void make_frame() {
    // Construct
    frame = (new BlockFrame(FRAME_TILE_ID, FRAME_TEXTURE, Material::stone))->self;

    // Init
    frame->setShape(0, 0, 0, 1, 0.5f, 1);
    frame->init();
    frame->setDestroyTime(2.0f);
    frame->setExplodeable(5.0f);
    frame->setSoundType(Tile::SOUND_WOOD);
    frame->category = 1;
    frame->setDescriptionId("frame");
}
