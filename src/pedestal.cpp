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
    PedestalTileEntity *pedestal_te = (PedestalTileEntity *) Level_getTileEntity(level, x, y, z);
    ItemInstance *held = Inventory_getSelected(player->inventory);
    if (ItemInstance_isNull(&pedestal_te->item)) {
        // Take from player
        if (held && held->count) {
            pedestal_te->item = {.count = 1, .id = held->id, .auxiliary = held->auxiliary};
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
        if (!inventory->vtable->add(inventory, &pedestal_te->item)) {
            // Drop on the ground
            ItemEntity *item_entity = (ItemEntity *) EntityFactory_CreateEntity(64, level);
            ALLOC_CHECK(item_entity);
            ItemEntity_constructor(item_entity, level, x + 0.5f, y + 1, z + 0.5f, &pedestal_te->item);
            Entity_moveTo_non_virtual((Entity *) item_entity, x + 0.5f, y + 1, z + 0.5f, 0, 0);
            Level_addEntity(level, (Entity *) item_entity);
        }
        pedestal_te->item = {0, 0, 0};
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
    pedestal = new EntityTile();
    ALLOC_CHECK(pedestal);
    int texture = INVALID_TEXTURE;
    Tile_constructor((Tile *) pedestal, PEDESTAL_ID, texture, Material_glass);
    Tile_isEntityTile[PEDESTAL_ID] = true;
    pedestal->texture = texture;

    // Set VTable
    pedestal->vtable = dup_EntityTile_vtable(EntityTile_vtable_base);
    ALLOC_CHECK(pedestal->vtable);
    pedestal->vtable->newTileEntity = Pedestal_newTileEntity;
    pedestal->vtable->use = Pedestal_use;
    pedestal->vtable->getRenderShape = Pedestal_getRenderShape;
    pedestal->vtable->isSolidRender = Pedestal_isSolidRender;
    pedestal->vtable->getRenderLayer = Pedestal_getRenderLayer;
    pedestal->vtable->isCubeShaped = Pedestal_isCubeShaped;

    // Init
    EntityTile_init(pedestal);
    pedestal->vtable->setDestroyTime(pedestal, 2.0f);
    pedestal->vtable->setExplodeable(pedestal, 10.0f);
    pedestal->vtable->setSoundType(pedestal, &Tile_SOUND_STONE);
    pedestal->category = 4;
    std::string name = "pedestal";
    pedestal->vtable->setDescriptionId(pedestal, &name);
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
    TileEntity_load_non_virtual(self, tag);
    CompoundTag *ctag = CompoundTag_getCompound_but_not(tag, "Item");
    if (ctag) {
        ItemInstance *i = ItemInstance_fromTag(ctag);
        if (i) {
            ((PedestalTileEntity *) self)->item = *i;
            delete i;
        }
    }
}

static bool PedestalTileEntity_save(TileEntity *self, CompoundTag *tag) {
    TileEntity_save_non_virtual(self, tag);
    CompoundTag *ctag = new CompoundTag();
    CompoundTag_constructor(ctag, "");
    ItemInstance_save(&((PedestalTileEntity *) self)->item, ctag);
    std::string item = "Item";
    CompoundTag_put(tag, &item, (Tag *) ctag);
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
    TileEntity_constructor(pedestal_te, 47);
    pedestal_te->item = {0, 0, 0};
    pedestal_te->renderer_id = 10;
    pedestal_te->vtable = get_pedestal_te_vtable();
    return pedestal_te;
}

HOOK_FROM_CALL(0xd2544, TileEntity *, TileEntityFactory_createTileEntity, (int id)) {
    if (id == 47) {
        return make_pedestal_tile_entity();
    }
    // Call original
    return TileEntityFactory_createTileEntity_original_FG6_API(id);
}

HOOK_FROM_CALL(0x149b0, void, TileEntity_initTileEntities, ()) {
    // Call original
    TileEntity_initTileEntities_original_FG6_API();
    // Add
    std::string str = "Pedestal";
    TileEntity_setId(47, &str);
}

// Tile again
static TileEntity *Pedestal_newTileEntity(UNUSED EntityTile *self) {
    return make_pedestal_tile_entity();
}

// Renderer
static void PedestalTileEntityRenderer_render(UNUSED TileEntityRenderer *self, TileEntity *tileentity, float x, float y, float z, UNUSED float unknown) {
    PedestalTileEntity *pedestal_te = (PedestalTileEntity *) tileentity;
    // Render the item
    if (ItemInstance_isNull(&pedestal_te->item)) return;
    glPushMatrix();
    float bob = 0.1f * sinf(5.0f * the_spinny * (M_PI / 180.0f));
    glTranslatef(x + 0.5f, y + 1.3f + bob, z + 0.5f);
    glRotatef(the_spinny, 0, 1, 0);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    std::string file = "terrain.png";
    EntityRenderer_bindTexture(NULL, &file);
    glScalef(0.25f, 0.25f, 0.25f);
    ItemInHandRenderer_renderItem(EntityRenderer_entityRenderDispatcher->item_renderer, NULL, &pedestal_te->item);
    glPopMatrix();
}

CUSTOM_VTABLE(pedestal_ter, TileEntityRenderer) {
    vtable->render = PedestalTileEntityRenderer_render;
}

static TileEntityRenderer *make_pedestal_tile_entity_renderer() {
    TileEntityRenderer *pedestal_ter = new TileEntityRenderer;
    ALLOC_CHECK(pedestal_ter);
    pedestal_ter->vtable = get_pedestal_ter_vtable();
    return pedestal_ter;
}

HOOK_FROM_CALL(0x67330, void, TileEntityRenderDispatcher_constructor, (TileEntityRenderDispatcher *self)) {
    // Call original
    TileEntityRenderDispatcher_constructor_original_FG6_API(self);
    // Add pedestal renderer
    TileEntityRenderer *pedestalTileEntityRenderer = make_pedestal_tile_entity_renderer();
    self->renderer_map.insert(std::make_pair(10, pedestalTileEntityRenderer));
}
