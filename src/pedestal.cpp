#include <cmath>

#include <GLES/gl.h>

//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "init.h"
#include "pedestal.h"
#include "nether_wand.h"

// Tile entity
static float the_spinny = 1.0f;
static void mcpi_callback(UNUSED Minecraft *mc) {
    the_spinny += 1;
    if (the_spinny > 360) the_spinny -= 360;
}

__attribute__((constructor)) static void init() {
    // For the spinny
    misc_run_on_tick(mcpi_callback);
}

struct PedestalTileEntity final : CustomTileEntity {
    ItemInstance item;

    PedestalTileEntity(int id) : CustomTileEntity(id), item({0, 0, 0}) {
        this->self->renderer_id = 10;
    }

    bool shouldSave() override {
        return true;
    }

    void load(CompoundTag *tag) override {
        TileEntity_load->get(false)(self, tag);
        CompoundTag *ctag = CompoundTag_getCompound_but_not(tag, "Item");
        if (ctag) {
            ItemInstance *i = ItemInstance::fromTag(ctag);
            if (i) {
                this->item = *i;
                delete i;
            }
        }
    }

    bool save(CompoundTag *tag) override {
        TileEntity_save->get(false)(self, tag);
        CompoundTag *ctag = CompoundTag::allocate();
        ctag->constructor("");
        this->item.save(ctag);
        std::string item = "Item";
        tag->put(item, (Tag *) ctag);
        return true;
    }

    void tick() override {}
};

ItemInstance &get_pedestal_item(PedestalTileEntity *pedestal) {
    return pedestal->item;
}

OVERWRITE_CALLS(TileEntityFactory_createTileEntity, TileEntity *, TileEntityFactory_createTileEntity_injection, (TileEntityFactory_createTileEntity_t original, int id)) {
    if (id == 47) {
        return (TileEntity *) (new PedestalTileEntity(47))->self;;
    }
    // Call original
    return original(id);
}

OVERWRITE_CALLS(TileEntity_initTileEntities, void, TileEntity_initTileEntities_injection, (TileEntity_initTileEntities_t original)) {
    // Call original
    original();
    // Add
    std::string str = "Pedestal";
    TileEntity::setId(47, str);
}

// Renderer
struct PedestalTileEntityRenderer : CustomTileEntityRenderer {
    PedestalTileEntityRenderer() : CustomTileEntityRenderer() {}

    void render(TileEntity *_tileentity, float x, float y, float z, UNUSED float unknown) override {
        PedestalTileEntity *tileentity = (PedestalTileEntity *) custom_get<CustomTileEntity>(_tileentity);
        // Render the item
        if (tileentity->item.isNull()) return;
        media_glPushMatrix();
        float bob = 0.1f * sinf(5.0f * the_spinny * (M_PI / 180.0f));
        media_glTranslatef(x + 0.5f, y + 1.3f + bob, z + 0.5f);
        media_glRotatef(the_spinny, 0, 1, 0);
        media_glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        std::string file = "terrain.png";
        EntityRenderer_bindTexture->get(false)(NULL, file);
        media_glScalef(0.25f, 0.25f, 0.25f);
        EntityRenderer::entityRenderDispatcher->item_renderer->renderItem(nullptr, &tileentity->item);
        media_glPopMatrix();
    }
};

OVERWRITE_CALLS(
    TileEntityRenderDispatcher_constructor,
    TileEntityRenderDispatcher *, TileEntityRenderDispatcher_constructor_injection, (TileEntityRenderDispatcher_constructor_t original, TileEntityRenderDispatcher *self)
) {
    // Call original
    original(self);
    // Add pedestal renderer
    TileEntityRenderer *pedestalTileEntityRenderer = (new PedestalTileEntityRenderer())->self;
    self->renderer_map.insert(std::make_pair(10, pedestalTileEntityRenderer));
    return self;
}

// Tile
static EntityTile *pedestal = NULL;
static TileEntity *Pedestal_newTileEntity(UNUSED EntityTile *self);
struct Pedestal : CustomEntityTile {
    Pedestal(const int id, const int texture, const Material *material): CustomEntityTile(id, texture, material) {}

    bool use(Level *level, int x, int y, int z, Player *player) override {
        // This will implicitly make the tile entity if it doesn't exist, awesome!
        PedestalTileEntity *tileentity = (PedestalTileEntity *) custom_get<CustomTileEntity>(level->getTileEntity(x, y, z));
        ItemInstance *held = player->inventory->getSelected();
        if (tileentity->item.isNull()) {
            // Take from player
            if (held && held->count) {
                tileentity->item = {.count = 1, .id = held->id, .auxiliary = held->auxiliary};
                if (!player->abilities.infinite_items) {
                    held->count -= 1;
                }
            }
        } else if (held && held->id == NETHER_WAND_ID && transmutate_pedestal(level, player, x, y, z, held)) {
            // Transmutate
            if (!player->abilities.infinite_items) {
                held->auxiliary -= 1;
            }
        } else {
            // Give to the player
            Inventory *inventory = player->inventory;
            if (!inventory->add(&tileentity->item)) {
                // Drop on the ground
                ItemEntity *item_entity = (ItemEntity *) EntityFactory::CreateEntity(64, level);
                item_entity->constructor(level, x + 0.5f, y + 1, z + 0.5f, tileentity->item);
                item_entity->moveTo(x + 0.5f, y + 1, z + 0.5f, 0, 0);
                level->addEntity((Entity *) item_entity);
            }
            tileentity->item = {0, 0, 0};
        }
        return true;
    }

    int getRenderShape() override {
        // Hack for gui renderering
        return 49 * (self && self->y1 == 0 && self->y2 == 1);
    }

    bool isSolidRender() override {
        // Stop it from turning other blocks invisable
        return 0;
    }

    int getRenderLayer() override {
        // Stop weird transparency issues
        return 1;
    }

    bool isCubeShaped() override {
        return false;
    }

    TileEntity *newTileEntity() {
        return (TileEntity *) (new PedestalTileEntity(47))->self;
    }
};

void make_pedestal() {
    // Construct
    pedestal = (new Pedestal(PEDESTAL_ID, INVALID_TEXTURE, Material::glass))->self;

    // Init
    pedestal->init();
    pedestal->setDestroyTime(2.0f);
    pedestal->setExplodeable(10.0f);
    pedestal->setSoundType(Tile::SOUND_STONE);
    pedestal->category = 4;
    std::string name = "pedestal";
    pedestal->setDescriptionId(name);
}
