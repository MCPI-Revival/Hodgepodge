#include <cmath>

//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>

#include "api.h"
#include "path.h"

static Tile *path = NULL;

// Interaction
static int GrassBlock_use(UNUSED Tile *tile, Level *level, int x, int y, int z, Player *player) {
}

__attribute__((constructor)) static void init() {
    patch_address((void *) ItemEntity_playerTouch_vtable_addr, (void *) ItemEntity_playerTouch_injection);
}

// Makes the path
void make_path() {
    // Construct
    path = new Tile();
    ALLOC_CHECK(path);
    int texture = PATH_ITEM_TEXTURE;
    Tile_constructor(path, PATH_ID, texture, Material_metal);
    path->texture = texture;
    ConveyorBelt_updateDefaultShape(path);

    // Set VTable
    // Copies GrassTile_vtable
    path->vtable = dup_Tile_vtable((Tile_vtable_t *) 0x1115a8);
    ALLOC_CHECK(path->vtable);

    // Modify functions
    path->vtable->getTexture2 = ConveyorBelt_getTexture2;
    path->vtable->getTexture2 = ConveyorBelt_getTexture2;
    path->vtable->use = Tile_use;

    // Init
    Tile_init(path);
    path->vtable->setDestroyTime(path, 2.0f);
    path->vtable->setExplodeable(path, 10.0f);
    path->category = 4;
    std::string name = "Path";
    path->vtable->setDescriptionId(path, &name);
}
