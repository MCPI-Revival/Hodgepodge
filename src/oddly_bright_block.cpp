#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>

#include "api.h"
#include "oddly_bright_block.h"

static Tile *oddlybrightblock = NULL;

static int OddlyBrightBlock_getRenderShape(UNUSED Tile *tile) {
    return 48;
}

static int OddlyBrightBlock_getTexture2(UNUSED Tile *tile, int face, int data) {
    static Tile_getTexture2_t ClothTile_getTexture2 = (Tile_getTexture2_t) 0xca22c;
    return ClothTile_getTexture2(tile, face, data);
}

void make_oddly_bright_blocks() {
    // Construct
    oddlybrightblock = alloc_Tile();
    ALLOC_CHECK(oddlybrightblock);
    int texture = 4 * 16; //OBB_ITEM_TEXTURE;
    Tile_constructor(oddlybrightblock, OBB_ID, texture, Material_metal);
    oddlybrightblock->texture = texture;

    // Set VTable
    oddlybrightblock->vtable = dup_Tile_vtable(Tile_vtable_base);
    ALLOC_CHECK(oddlybrightblock->vtable);
    oddlybrightblock->vtable->getRenderShape = OddlyBrightBlock_getRenderShape;
    oddlybrightblock->vtable->getTexture2 = OddlyBrightBlock_getTexture2;

    // Init
    Tile_init(oddlybrightblock);
    oddlybrightblock->vtable->setDestroyTime(oddlybrightblock, 2.0f);
    oddlybrightblock->vtable->setExplodeable(oddlybrightblock, 10.0f);
    oddlybrightblock->vtable->setLightEmission(oddlybrightblock, 1.0f);
    oddlybrightblock->category = 4;
    std::string name = "Oddly Bright Block";
    oddlybrightblock->vtable->setDescriptionId(oddlybrightblock, &name);
}
