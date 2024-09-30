#if 0
#include <symbols/minecraft.h>
#include <libreborn/libreborn.h>
#include <mods/misc/misc.h>

#include "init.h"
#include "api.h"

#define VINE_ID 66
#define VINE_TEXTURE 143

Tile_getTexture2_t TreeTile_getTexture2_original_FG6_API = NULL;
int TreeTile_getTexture2_injection(Tile *t, int face, int data) {
    if (data == 3 && face != 1 && face != 0) return 153;
    return TreeTile_getTexture2_original_FG6_API(t, face, data);
}

void Inventory_setupDefault_FillingContainer_addItem_birch_log_injection(FillingContainer *self, ItemInstance *i) {
    FillingContainer_addItem(self, i);
    ItemInstance *ii = new ItemInstance;
    *ii = *i;
    ii->auxiliary++;
    FillingContainer_addItem(self, ii);
}

static void _place_vines(Level *level, int x, int y, int z, int meta, Random *random) {
    // Offset from meta
    switch (meta) {
        case 2:
            z -= 1;
            break;
        case 3:
            z += 1;
            break;
        case 4:
            x -= 1;
            break;
        case 5:
            x += 1;
            break;
    }
    if (Level_getTile_non_virtual(level, x, y, z) != 0) return;
    // Calc len
    int len = Random_genrand_int32(random) % 5;
    if (len == 0) return;
    for (int i = 0; i <= len; i++) {
        Level_setTileAndDataNoUpdate(level, x, y - i, z, VINE_ID, meta);
    }
}

static void place_vines(Level *level, int x, int y, int z, Random *random) {
    _place_vines(level, x, y, z, 2, random);
    _place_vines(level, x, y, z, 3, random);
    _place_vines(level, x, y, z, 4, random);
    _place_vines(level, x, y, z, 5, random);
}

static void place_leaves(Level *level, int x, int y, int z, int r, Random *random) {
    bool lastFailed = false;
    for(int lz = -r; lz <= r; lz++) {
        for(int lx = -r; lx <= r; lx++) {
            int lxm = lx - 1;
            int lzm = lz - 1;
            int r2 = r * r;
            if (
                lx*lx   + lz*lz   > r2 ||
                lx*lx   + lzm*lzm > r2 ||
                lxm*lxm + lz*lz   > r2 ||
                lxm*lxm + lzm*lzm > r2
            ) {
                lastFailed = true;
                continue;
            }
            lastFailed = false;
            int id = Level_getTile_non_virtual(level, x + lx, y, z + lz);
            if (id == 0 || id == VINE_ID) {
                Level_setTileAndData(level, x + lx, y, z + lz, 18, 3);
            }
            // Place vines
            if (lx == -r || lx == r || lz == -r || lz == r || lastFailed) {
                place_vines(level, x, y, z, random);
            }
        }
    }
}

bool JungleTreeFeature_place(UNUSED Feature *self, Level *level, Random *random, int x, int y, int z) {
    // Check ground
    if (
        Level_getTile_non_virtual(level, x,     y - 1, z    ) != 2 ||
        Level_getTile_non_virtual(level, x,     y - 1, z + 1) != 2 ||
        Level_getTile_non_virtual(level, x + 1, y - 1, z    ) != 2 ||
        Level_getTile_non_virtual(level, x + 1, y - 1, z + 1) != 2
    ) return false;
    // Place trunk and vines
    int size = 29 + (Random_genrand_int32(random) % 3);
    if (size + y + 3 > 128) return false;
    for (int i = 0; i <= size; i++) {
        int vines = Random_genrand_int32(random) % 256;
        Level_setTileAndDataNoUpdate(level, x,     y + i, z,     17, 3);
        if (vines & (1 << 0)) Level_setTileAndDataNoUpdate(level, x - 1, y + i, z,     VINE_ID, 4);
        if (vines & (1 << 1)) Level_setTileAndDataNoUpdate(level, x,     y + i, z - 1, VINE_ID, 2);
        vines >>= 2;
        Level_setTileAndDataNoUpdate(level, x,     y + i, z + 1, 17, 3);
        if (vines & (1 << 0)) Level_setTileAndDataNoUpdate(level, x - 1, y + i, z + 1, VINE_ID, 4);
        if (vines & (1 << 1)) Level_setTileAndDataNoUpdate(level, x,     y + i, z + 2, VINE_ID, 3);
        vines >>= 2;
        Level_setTileAndDataNoUpdate(level, x + 1, y + i, z,     17, 3);
        if (vines & (1 << 0)) Level_setTileAndDataNoUpdate(level, x + 2, y + i, z,     VINE_ID, 5);
        if (vines & (1 << 1)) Level_setTileAndDataNoUpdate(level, x + 1, y + i, z - 1, VINE_ID, 2);
        vines >>= 2;
        Level_setTileAndDataNoUpdate(level, x + 1, y + i, z + 1, 17, 3);
        if (vines & (1 << 0)) Level_setTileAndDataNoUpdate(level, x + 2, y + i, z + 1, VINE_ID, 5);
        if (vines & (1 << 1)) Level_setTileAndDataNoUpdate(level, x + 1, y + i, z + 2, VINE_ID, 3);
    }
    // Place leaves
    place_leaves(level, x, y + size - 1, z, 6, random);
    place_leaves(level, x, y + size + 0, z, 5, random);
    place_leaves(level, x, y + size + 1, z, 4, random);
    return true;
}
CUSTOM_VTABLE(jungle_tree_feature, Feature) {
    vtable->place = JungleTreeFeature_place;
}
static Feature *create_jungle_tree_feature() {
    Feature *jtf = new Feature;
    Feature_constructor(jtf);
    jtf->vtable = get_jungle_tree_feature_vtable();
    return jtf;
}

Tile *vines = NULL;
static Tile_vtable *vines_vtable = NULL;
static AABB *Vines_getAABB(UNUSED Tile *self, UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) {
    return NULL;
}

static Tile_mayPlace_t Ladder_mayPlace = NULL;
static bool Vines_mayPlace(Tile *self, Level *level, int x, int y, int z, uchar face) {
    if (face == 0 && level->getTile(x, y, z) == self->id) {
        return true;
    }
    return Ladder_mayPlace(self, level, x, y, z, face);
}

static Tile_neighborChanged_t Ladder_neighborChanged = NULL;
static void Vines_neighborChanged(Tile *self, Level *level, int x, int y, int z, int type) {
    if (
        level->vtable->getTile(level, x, y + 1, z) == self->id &&
        level->vtable->getData(level, x, y + 1, z) == level->vtable->getData(level, x, y, z)
    ) {
        // It's fine
        return;
    }
    Ladder_neighborChanged(self, level, x, y, z, type);
}

static bool &is_rendering_vine() {
    static bool _ = false;
    return _;
}

float TileRenderer_tesselateLadderInWorld_Tile_getBrightness(Tile *self, LevelSource *level, int x, int y, int z) {
    is_rendering_vine() = self->id == VINE_ID;
    return self->vtable->getBrightness(self, level, x, y, z);
}

// Force TileRenderer_tesselateLadderInWorld to use color
void TileRenderer_tesselateLadderInWorld_Tesselator_color(Tesselator *t, float r, float g, float b) {
    if (is_rendering_vine() == true) {
        is_rendering_vine() = false;
        r = 0;
        b = 0;
    }
    Tesselator_color(t, r * 255, g * 255, b * 255, 0xff);
}

static void create_vines() {
    if (vines_vtable == NULL) {
        vines_vtable = (Tile_vtable *) malloc(TILE_VTABLE_SIZE);
        memcpy((void *) vines_vtable, (void *) 0x1138b8, TILE_VTABLE_SIZE);
        vines_vtable->getAABB = Vines_getAABB;
        Ladder_mayPlace = vines_vtable->mayPlace;
        vines_vtable->mayPlace = Vines_mayPlace;
        Ladder_neighborChanged = vines_vtable->neighborChanged;
        vines_vtable->neighborChanged = Vines_neighborChanged;
    }
    vines = new Tile();
    int texture = VINE_TEXTURE;
    Tile_constructor(vines, VINE_ID, texture, Material_plant);
    vines->vtable = vines_vtable;
    Tile_init(vines);
    std::string name = "vines";
    vines->vtable->setDescriptionId(vines, &name);
    vines->vtable->setSoundType(vines, &Tile_SOUND_GRASS);
}

static void Tile_initTiles_injection(UNUSED void *null) {
    create_vines();
}

int Mob_onLadder_Level_getTile_injection(Level *level, int x, int y, int z) {
    int ret = level->vtable->getTile(level, x, y, z);
    if (ret == VINE_ID) ret = 65;
    return ret;
}

void*foo(){return*(void**)0x17c944;}
__attribute__((constructor)) static void init() {
    // Add texture to log aux
    TreeTile_getTexture2_original_FG6_API = *(Tile_getTexture2_t *) 0x1126d4;
    patch_address((void *) 0x1126d4, (void *) TreeTile_getTexture2_injection);

    // Add to inventory right after birch
    overwrite_call((void *) 0x8d484, (void *) Inventory_setupDefault_FillingContainer_addItem_birch_log_injection);

    // The tree feature
    patch_address((void *) 0x10ffa8, (void *) create_jungle_tree_feature);

    // Temporary, makes all biomes rainforests
    overwrite_calls((void *) 0xadf04, (void *) foo);

    // Makes vines climbable
    overwrite_call((void *) 0x7f938, (void *) Mob_onLadder_Level_getTile_injection);
    overwrite_call((void *) 0x7f964, (void *) Mob_onLadder_Level_getTile_injection);

    // Make TileRenderer_tesselateLadderInWorld use color
    overwrite_call((void *) 0x53edc, (void *) TileRenderer_tesselateLadderInWorld_Tile_getBrightness);
    overwrite_call((void *) 0x53eec, (void *) TileRenderer_tesselateLadderInWorld_Tesselator_color);

    misc_run_on_tiles_setup(Tile_initTiles_injection);
}
#endif