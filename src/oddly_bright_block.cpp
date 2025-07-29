//#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>

#include "api.h"
#include "oddly_bright_block.h"

static Tile *oddlybrightblock = NULL;

struct OddlyBrightBlock final : CustomTile {
    OddlyBrightBlock(const int id, const int texture, const Material *material): CustomTile(id, texture, material) {}

    int getRenderShape() override {
        return 48;
    }

    int getTexture2(int face, int data) override {
        return ClothTile_getTexture2(self, face, data);
    }
};

void make_oddly_bright_blocks() {
    // Construct
    oddlybrightblock = (new OddlyBrightBlock(OBB_ID, 4 * 16 /*OBB_ITEM_TEXTURE*/, Material::metal))->self;

    // Init
    oddlybrightblock->init();
    oddlybrightblock->setDestroyTime(2.0f);
    oddlybrightblock->setExplodeable(10.0f);
    oddlybrightblock->setLightEmission(1.0f);
    oddlybrightblock->category = 4;
    std::string name = "Oddly Bright Block";
    oddlybrightblock->setDescriptionId(name);
}
