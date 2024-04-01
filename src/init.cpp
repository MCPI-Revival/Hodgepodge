#include <math.h>

#include <libreborn/libreborn.h>
#include <symbols/minecraft.h>
#include <mods/misc/misc.h>
#include <mods/title-screen/title-screen.h>

#include "api.h"
//#include "achievements.h"
#include "piston.h"
#include "bomb.h"
#include "ender_pearls.h"
#include "nether_wand.h"
#include "pedestal.h"
#include "belt.h"
#include "dash.h"
#include "block_frame.h"
#include "init.h"
#include "redstone.h"
//#include "inventory.h"
#include "oddly_bright_block.h"

#ifdef FISH
#include <mods/compat/compat.h>
#endif

Minecraft *mc = NULL;

// Tick speed stuff
/*typedef void (*Timer_advanceTime_t)(int *param_1);
Timer_advanceTime_t Timer_advanceTime = (Timer_advanceTime_t) 0x17e08;
static int last = 0;
static int speedup = 1;
static void Timer_advanceTime_injection(int *param_1) {
    Timer_advanceTime(param_1);
    if (param_1[1] && last == 0) {
        param_1[1] = 1;
        last += 1;
    } else if (param_1[1]) {
        param_1[1] = 0;
        last += 1;
    }
    last %= speedup;
}*/
static void mcpi_callback(Minecraft *mcpi) {
    mc = mcpi;
    /*if (achievement_wants_open) {
        if (mcpi->player && mcpi->level && !mcpi->screen) {
            Minecraft_setScreen(mcpi, create_achievement_screen());
        }
        achievement_wants_open = false;
    } else if (hotbar_needs_update) {
        if (mcpi->player && mcpi->level && (mcpi->screen == NULL || mcpi->screen->vtable == get_inventory_screen_vtable())) {
            set_hotbar(mcpi->player->inventory);
        } else {
            hotbar_offset -= 1;
            if (hotbar_offset == -1) hotbar_offset = 3;
        }
        hotbar_needs_update = 0;
    }
    // Tick stuff
    if (last) {
        Minecraft_tickInput(mc);
        if (mc->level) {
            GameRenderer_tick(mc->gamerenderer, 1, 1);
            LevelRenderer_tick(mc->levelrenderer);
            if (mc->player) {
                Level_animateTick(mc->level, floor(mc->player->x), floor(mc->player->y), floor(mc->player->z));
            }
        }
    }*/
}

// Add everything to the creative inventory
#define ADD_ITEM_AUX(item_id, aux) do { \
        ItemInstance *ii = new ItemInstance; \
        ALLOC_CHECK(ii); \
        ii->count = 255; \
        ii->auxiliary = aux; \
        ii->id = item_id; \
        FillingContainer_addItem(filling_container, ii); \
    } while (false)
#define ADD_ITEM(item_id) ADD_ITEM_AUX(item_id, 0)

static void Inventory_setupDefault_FillingContainer_addItem_call_injection(FillingContainer *filling_container) {
    ADD_ITEM(REDSTONE_ID);
    //ADD_ITEM(75);
    //ADD_ITEM(REPEATER_ID);
    ADD_ITEM(BELT_ID);
    ADD_ITEM(PEDESTAL_ID);
    ADD_ITEM(ENDER_PEARL_ITEM_ID);
    ADD_ITEM(BOMB_ITEM_ID);
    ADD_ITEM(DASH_ITEM_ID);
    ADD_ITEM(NETHER_WAND_ID);
    //ADD_ITEM(FRAME_TILE_ID);
    // Piston
    ADD_ITEM(33);
    // Sticky piston
    ADD_ITEM(29);
    // Moving piston
    //ADD_ITEM(36);
    // Redstone block
    ADD_ITEM(152);
    // OBBs
    /*for (int i = 0; i < 16; i++) {
        ADD_ITEM_AUX(OBB_ID, i);
    }*/
}

// Recipe
#define RECIPE_ITEM(name, c, id_, aux) Recipes_Type name={.item=0, .tile=0, .instance={.count=0, .id=(id_), .auxiliary=(aux)}, .letter=(c)};
static void Recipes_injection(Recipes *recipes) {
    // ConveyorBelt
    RECIPE_ITEM(iron, 'i', 265, 0);
    RECIPE_ITEM(stone, 's', 1, 0);
    RECIPE_ITEM(leather, 'l', 334, 0);
    ItemInstance conveyorbelt_item = {
        .count = 8,
        .id = BELT_ID,
        .auxiliary = 0
    };
    // Add
    std::string line1 = "   ";
    std::string line2 = "lll";
    std::string line3 = "isi";
    std::vector<Recipes_Type> ingredients = {iron, stone, leather};
    Recipes_addShapedRecipe_3(recipes, &conveyorbelt_item, &line1, &line2, &line3, &ingredients);

    // Nether wand
    RECIPE_ITEM(gold, 'g', 266, 0);
    RECIPE_ITEM(nether_core, 'N', 247, 0);
    ItemInstance netherwand_item = {
        .count = 1,
        .id = NETHER_WAND_ID,
        .auxiliary = 0
    };
    line1 = "  N";
    line2 = " g ";
    line3 = "g  ";
    ingredients = {gold, nether_core};
    Recipes_addShapedRecipe_3(recipes, &netherwand_item, &line1, &line2, &line3, &ingredients);

    // Bomb
    RECIPE_ITEM(gunpowder, 'G', 289, 0);
    RECIPE_ITEM(string, 's', 287, 0);
    ItemInstance bomb_item = {
        .count = 1,
        .id = BOMB_ITEM_ID,
        .auxiliary = 0
    };
    line1 = "   ";
    line2 = " s ";
    line3 = "G  ";
    ingredients = {gunpowder, string};
    Recipes_addShapedRecipe_3(recipes, &bomb_item, &line1, &line2, &line3, &ingredients);

    // Pedestal
    // TODO: Recipes for other quartz stuff
    RECIPE_ITEM(quartz, 'Q', 406, 0);
    ItemInstance pedestal_item = {
        .count = 1,
        .id = PEDESTAL_ID,
        .auxiliary = 0
    };
    line1 = "QQQ";
    line2 = " Q ";
    line3 = " Q ";
    ingredients = {quartz};
    Recipes_addShapedRecipe_3(recipes, &pedestal_item, &line1, &line2, &line3, &ingredients);

    // Piston
    RECIPE_ITEM(cobble, 'c', 4, 0);
    RECIPE_ITEM(planks, 'p', 5, 0);
    RECIPE_ITEM(redstone, 'r', REDSTONE_ID, 0);
    ItemInstance piston_item = {
        .count = 1,
        .id = 33,
        .auxiliary = 0
    };
    line1 = "ppp";
    line2 = "cic";
    line3 = "crc";
    ingredients = {cobble, planks, redstone, iron};
    Recipes_addShapedRecipe_3(recipes, &piston_item, &line1, &line2, &line3, &ingredients);

    // Sticky piston
    RECIPE_ITEM(piston, 'P', 33, 0);
    RECIPE_ITEM(slime, 's', 341, 0);
    ItemInstance spiston_item = {
        .count = 1,
        .id = 29,
        .auxiliary = 0
    };
    line1 = "   ";
    line2 = " s ";
    line3 = " P ";
    ingredients = {piston, slime};
    Recipes_addShapedRecipe_3(recipes, &spiston_item, &line1, &line2, &line3, &ingredients);

    // Redstone
    RECIPE_ITEM(rblock, 'R', 152, 0);
    ItemInstance redstone_item = {
        .count = 9,
        .id = REDSTONE_ID,
        .auxiliary = 0
    };
    line1 = "R  ";
    line2 = "   ";
    line3 = "   ";
    ingredients = {rblock};
    Recipes_addShapedRecipe_3(recipes, &redstone_item, &line1, &line2, &line3, &ingredients);

    // Redstone Block
    ItemInstance rblock_item = {
        .count = 9,
        .id = 152,
        .auxiliary = 0
    };
    line1 = "rrr";
    line2 = "rrr";
    line3 = "rrr";
    ingredients = {redstone};
    Recipes_addShapedRecipe_3(recipes, &rblock_item, &line1, &line2, &line3, &ingredients);
}

static void Tile_initTiles_injection(UNUSED void *null) {
    make_conveyorbelt();
    make_oddly_bright_blocks();
    make_pedestal();
    make_frame();
    make_pistons();
    make_redstone_blocks();
    make_redstone_wire();
}

static void Item_initItems_injection(UNUSED void *null) {
    make_ender_pearl();
    make_nether_wand();
    make_bomb();
    make_dash();
    //make_redstone();
}

static void Language_injection(__attribute__((unused)) void *null) {
    // Custom stuff
    // TODO: Fix desc strings
    I18n__strings.insert(std::make_pair("item.ender_pearl.name", "Ender Pearl"));
    I18n__strings.insert(std::make_pair("desc.ender_pearl", "Throw it to teleport"));
    I18n__strings.insert(std::make_pair("item.redstoneDust.name", "Redstone Dust"));
    I18n__strings.insert(std::make_pair("desc.redstoneDust", "Don't get your hopes up"));
    I18n__strings.insert(std::make_pair("item.bomb.name", "Bomb"));
    I18n__strings.insert(std::make_pair("desc.bomb", "Do you want to explode?"));
    I18n__strings.insert(std::make_pair("item.dash.name", "Thrust Crystal"));
    I18n__strings.insert(std::make_pair("desc.dash", "Thrust forward for a small amount of damage"));
    I18n__strings.insert(std::make_pair("item.nether_wand.name", "Nether Wand"));
    I18n__strings.insert(std::make_pair("desc.nether_wand", "A strange and powerful wand, with the powers of transmutation"));
    I18n__strings.insert(std::make_pair("tile.ConveyorBelt.name", "Conveyor Belt"));
    //I18n__strings.insert(std::make_pair("desc.ConveyorBelt", "Move stuff around with this!"));
    I18n__strings.insert(std::make_pair("tile.Oddly Bright Block.name", "Oddly Bright Block"));
    //I18n__strings.insert(std::make_pair("desc.Oddly Bright Block", "Why is it so oddly bright?"));
    I18n__strings.insert(std::make_pair("tile.frame.name", "Block Frame"));
    // Desc: Allow you to shape blocks
    I18n__strings.insert(std::make_pair("tile.pedestal.name", "Pedestal"));
    // Desc: Just the thing for showing off your rock collection

    I18n__strings.insert(std::make_pair("tile.piston.name", "Piston"));
    I18n__strings.insert(std::make_pair("tile.pistonSticky.name", "Sticky Piston"));
    I18n__strings.insert(std::make_pair("tile.redstone_block.name", "Redstone Block"));

    // Lang fixes that needed changes in tiny.cpp
    I18n__strings.insert(std::make_pair("tile.waterStill.name", "Still Water"));
    I18n__strings.insert(std::make_pair("tile.lavaStill.name", "Still Lava"));
    I18n__strings.insert(std::make_pair("tile.grassCarried.name", "Carried Grass"));
    I18n__strings.insert(std::make_pair("tile.leavesCarried.name", "Carried Leaves"));
    I18n__strings.insert(std::make_pair("tile.invBedrock.name", "Invisible Bedrock"));
    // Lang fixes
    I18n__strings.insert(std::make_pair("item.camera.name", "Camera"));
    I18n__strings.insert(std::make_pair("item.seedsMelon.name", "Melon Seeds"));
    I18n__strings.insert(std::make_pair("tile.pumpkinStem.name", "Pumpkin Stem"));
    I18n__strings.insert(std::make_pair("tile.stoneSlab.name", "Double Stone Slab"));
}

#if !REBORN_HAS_LANG
I18n_loadLanguage_t I18n_loadLanguage_og = NULL;
static void I18n_loadLanguage_injection(AppPlatform *app, std::string language_name) {
    I18n_loadLanguage_og(app, language_name);
    Language_injection(NULL);
}
#endif

HOOK(title_screen_load_splashes, void, (std::vector<std::string> &splashes)) {
    ensure_title_screen_load_splashes();
    real_title_screen_load_splashes(splashes);
    // Add some cool splashes
    splashes.push_back("Type :music2: in chat!");
    splashes.push_back("The very model of a modern major general");
    splashes.push_back("Good guys, bad guys, and explosions!");
    splashes.push_back("Drinking lava is a feature not a bug");
    splashes.push_back("We *could* have backwards compat :(");
    splashes.push_back("I'll tell me ma!");
    splashes.push_back("Splish splash!");
    splashes.push_back("Now with REDSTONE?!?!");
    splashes.push_back("Go ye heros!");
    splashes.push_back("I've got a little list!");
    splashes.push_back("Poor wandering one");
    splashes.push_back("NO SOUND AT ALL!");
    splashes.push_back("'I didn't say that!' -TBR");
    splashes.push_back("I \3 extract_from_bl_instruction"); // `\3` is a heart
    splashes.push_back("Chestnace not included!"); // Or is it...?
    splashes.push_back("Work in progress!");
    splashes.push_back("Gold > Diamond!");
    splashes.push_back("Emeralds == Diamonds");
}

#ifdef FISH
static bool shown_screen = false;
static int license_check_thing() {
    if (/*!is_fish() ||*/ shown_screen) return 0;
    shown_screen = true;
    return 2;
}

static bool license_add_button() {
    return true;
}

static void restore_screen() {
    Minecraft_setScreen(mc, NULL);
}

static void InvalidLicenseScreen_init_injection(Screen *s) {
    typedef void (*InvalidLicenseScreen_init_t)(Screen *s);
    InvalidLicenseScreen_init_t InvalidLicenseScreen_init_og = (InvalidLicenseScreen_init_t) 0x39ff4;
    InvalidLicenseScreen_init_og(s);
    // Modify
    uchar *self = (uchar *) s;
    (*(Button **) (self + 0x5c))->text = "I want it to be real!";
}

static void line1_inj(std::string *s) {
    *s = "April Fools! :D";
}

static void line2_inj(std::string *s) {
    *s = "Did I trick you?";
}

static void line3_inj(std::string *s) {
    *s =
        "     Redstone would be very hard to port,\n"
        "only someone very smart (and good looking)\n"
        "                    could do it!";
}

static void nop(){}
#endif

__attribute__((constructor)) static void init() {
    //overwrite_calls((void *) 0x17e08, (void *) Timer_advanceTime_injection);
    misc_run_on_update(mcpi_callback);
    misc_run_on_tiles_setup(Tile_initTiles_injection);
    misc_run_on_items_setup(Item_initItems_injection);
    misc_run_on_recipes_setup(Recipes_injection);
    misc_run_on_creative_inventory_setup(Inventory_setupDefault_FillingContainer_addItem_call_injection);
#if REBORN_HAS_LANG
    misc_run_on_language_setup(Language_injection);
#else
    I18n_loadLanguage_og = (I18n_loadLanguage_t) extract_from_bl_instruction((uchar *) 0x149f0);
    overwrite_calls((void *) I18n_loadLanguage_og, (void *) I18n_loadLanguage_injection);
#endif

#ifdef FISH
    overwrite_calls((void *) 0x16e8c, (void *) license_check_thing);
    overwrite_call((void *) 0x39c38, (void *) license_add_button);
    overwrite_call((void *) 0x3e9cc, (void *) license_add_button);
    overwrite_call((void *) 0x39f10, (void *) compat_request_exit);
    overwrite_call((void *) 0x39f40, (void *) restore_screen);
    overwrite_calls((void *) 0x39ff4, (void *) InvalidLicenseScreen_init_injection)
    overwrite_call((void *) 0x3a140, (void *) nop);
    overwrite_call((void *) 0x3a14c, (void *) nop);
    overwrite_call((void *) 0x3a174, (void *) nop);
    overwrite_call((void *) 0x3a134, (void *) line1_inj);
    overwrite_call((void *) 0x3a15c, (void *) line2_inj);
    overwrite_call((void *) 0x3a168, (void *) line3_inj);
#endif
}
