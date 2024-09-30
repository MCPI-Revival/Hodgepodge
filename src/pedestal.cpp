#include <cmath>

#include <GLES/gl.h>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>

#include "api.h"
#include "init.h"
#include "pedestal.h"
#include "nether_wand.h"

// Tile
static EntityTile *pedestal = NULL;
static TileEntity *Pedestal_newTileEntity(UNUSED EntityTile *self);
static int Pedestal_use(UNUSED EntityTile *self, Level *level, int x, int y, int z, Player *player) {
    // This will implicitly make the tile entity, awesome!
    PedestalTileEntity *pedestal_te = (PedestalTileEntity *) level->getTileEntity(x, y, z);
    ItemInstance *held = player->inventory->getSelected();
    if (pedestal_te->data.item.isNull()) {
        // Take from player
        if (held && held->count) {
            pedestal_te->data.item = {.count = 1, .id = held->id, .auxiliary = held->auxiliary};
            if (!player->infinite_items) {
                held->count -= 1;
            }
        }
    } else if (held && held->id == NETHER_WAND_ID && transmutate_pedestal(level, player, x, y, z, held)) {
        // Transmutate
        if (!player->infinite_items) {
            held->auxiliary -= 1;
        }
    } else {
        // Give to the player
        Inventory *inventory = player->inventory;
        if (!inventory->add(&pedestal_te->data.item)) {
            // Drop on the ground
            ItemEntity *item_entity = (ItemEntity *) EntityFactory::CreateEntity(64, level);
            ALLOC_CHECK(item_entity);
            item_entity->constructor(level, x + 0.5f, y + 1, z + 0.5f, pedestal_te->data.item);
            item_entity->moveTo(x + 0.5f, y + 1, z + 0.5f, 0, 0);
            level->addEntity((Entity *) item_entity);
        }
        pedestal_te->data.item = {0, 0, 0};
    }
    return 1;
}

static int Pedestal_getRenderShape(UNUSED EntityTile *self) {
    // Hack for gui renderering
    return 49 * (self && self->y1 == 0 && self->y2 == 1);
}

static bool Pedestal_isSolidRender(UNUSED EntityTile *self) {
    // Stop it from turning other blocks invisable
    return 0;
}

static int Pedestal_getRenderLayer(UNUSED EntityTile *self) {
    // Stop weird transparency issues
    return 1;
}

static bool Pedestal_isCubeShaped(UNUSED EntityTile *self) {
    return false;
}

void make_pedestal() {
    // Construct
    pedestal = EntityTile::allocate();
    ALLOC_CHECK(pedestal);
    int texture = INVALID_TEXTURE;
    pedestal->constructor(PEDESTAL_ID, texture, Material::glass);

    // Set VTable
    pedestal->vtable = dup_vtable(EntityTile_vtable_base);
    ALLOC_CHECK(pedestal->vtable);
    pedestal->vtable->newTileEntity = Pedestal_newTileEntity;
    pedestal->vtable->use = Pedestal_use;
    pedestal->vtable->getRenderShape = Pedestal_getRenderShape;
    pedestal->vtable->isSolidRender = Pedestal_isSolidRender;
    pedestal->vtable->getRenderLayer = Pedestal_getRenderLayer;
    pedestal->vtable->isCubeShaped = Pedestal_isCubeShaped;

    // Init
    pedestal->init();
    pedestal->setDestroyTime(2.0f);
    pedestal->setExplodeable(10.0f);
    pedestal->setSoundType(Tile::SOUND_STONE);
    pedestal->category = 4;
    std::string name = "pedestal";
    pedestal->setDescriptionId(name);
}

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

static bool PedestalTileEntity_shouldSave(UNUSED TileEntity *self) {
    return true;
}

static void PedestalTileEntity_load(TileEntity *self, CompoundTag *tag) {
    TileEntity_load->get(false)(self, tag);
    CompoundTag *ctag = CompoundTag_getCompound_but_not(tag, "Item");
    if (ctag) {
        ItemInstance *i = ItemInstance::fromTag(ctag);
        if (i) {
            ((PedestalTileEntity *) self)->data.item = *i;
            delete i;
        }
    }
}

static bool PedestalTileEntity_save(TileEntity *self, CompoundTag *tag) {
    TileEntity_save->get(false)(self, tag);
    CompoundTag *ctag = CompoundTag::allocate();
    ctag->constructor("");
    ((PedestalTileEntity *) self)->data.item.save(ctag);
    std::string item = "Item";
    tag->put(item, (Tag *) ctag);
    return true;
}

static void PedestalTileEntity_tick(UNUSED TileEntity *self) {
}

CUSTOM_VTABLE(pedestal_te, TileEntity) {
    vtable->shouldSave = PedestalTileEntity_shouldSave;
    vtable->load = PedestalTileEntity_load;
    vtable->save = PedestalTileEntity_save;
    vtable->tick = PedestalTileEntity_tick;
}

static PedestalTileEntity *make_pedestal_tile_entity() {
    PedestalTileEntity *pedestal_te = new PedestalTileEntity;
    ALLOC_CHECK(pedestal_te);
    pedestal_te->super()->constructor(47);
    pedestal_te->data.item = {0, 0, 0};
    pedestal_te->super()->renderer_id = 10;
    pedestal_te->super()->vtable = get_pedestal_te_vtable();
    return pedestal_te;
}

OVERWRITE_CALLS(TileEntityFactory_createTileEntity, TileEntity *, TileEntityFactory_createTileEntity_injection, (TileEntityFactory_createTileEntity_t original, int id)) {
    if (id == 47) {
        return (TileEntity *) make_pedestal_tile_entity();
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

// Tile again
static TileEntity *Pedestal_newTileEntity(UNUSED EntityTile *self) {
    return (TileEntity *) make_pedestal_tile_entity();
}

// Renderer
static void PedestalTileEntityRenderer_render(UNUSED TileEntityRenderer *self, TileEntity *tileentity, float x, float y, float z, UNUSED float unknown) {
    PedestalTileEntity *pedestal_te = (PedestalTileEntity *) tileentity;
    // Render the item
    if (pedestal_te->data.item.isNull()) return;
    glPushMatrix();
    float bob = 0.1f * sinf(5.0f * the_spinny * (M_PI / 180.0f));
    glTranslatef(x + 0.5f, y + 1.3f + bob, z + 0.5f);
    glRotatef(the_spinny, 0, 1, 0);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    std::string file = "terrain.png";
    EntityRenderer_bindTexture->get(false)(NULL, file);
    glScalef(0.25f, 0.25f, 0.25f);
    EntityRenderer::entityRenderDispatcher->item_renderer->renderItem(nullptr, &pedestal_te->data.item);
    glPopMatrix();
}

CUSTOM_VTABLE(pedestal_ter, TileEntityRenderer) {
    vtable->render = PedestalTileEntityRenderer_render;
}

static TileEntityRenderer *make_pedestal_tile_entity_renderer() {
    TileEntityRenderer *pedestal_ter = TileEntityRenderer::allocate();
    ALLOC_CHECK(pedestal_ter);
    pedestal_ter->vtable = get_pedestal_ter_vtable();
    return pedestal_ter;
}

OVERWRITE_CALLS(TileEntityRenderDispatcher_constructor, TileEntityRenderDispatcher *, TileEntityRenderDispatcher_constructor_injection, (TileEntityRenderDispatcher_constructor_t original, TileEntityRenderDispatcher *self)) {
    // Call original
    original(self);
    // Add pedestal renderer
    TileEntityRenderer *pedestalTileEntityRenderer = make_pedestal_tile_entity_renderer();
    self->renderer_map.insert(std::make_pair(10, pedestalTileEntityRenderer));
    return self;
}
