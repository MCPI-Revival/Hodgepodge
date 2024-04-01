// Stores helper macros and symbols
#pragma once

#include <symbols/minecraft.h>
#include <cstring>

#define _HOOK_FROM_CALL(call, ret, name, args, fn_name, prefix) \
    prefix name##_t fn_name##_original = NULL; \
    prefix ret fn_name##_injection args; \
    __attribute__((constructor)) static void fn_name##_hooking_init() { \
        fn_name##_original = (name##_t) extract_from_bl_instruction((uchar *) call); \
        overwrite_calls((void *) fn_name##_original, (void *) fn_name##_injection); \
    } \
    prefix ret fn_name##_injection args
#define HOOK_FROM_CALL(call, ret, name, args) _HOOK_FROM_CALL(call, ret, name, args, name, static)

#define _HOOK_SINGLE_FROM_CALL(call, ret, name, args, fn_name, prefix) \
    prefix name##_t fn_name##_original = NULL; \
    prefix ret fn_name##_injection args; \
    __attribute__((constructor)) static void fn_name##_hooking_init() { \
        fn_name##_original = (name##_t) extract_from_bl_instruction((uchar *) call); \
        overwrite_call((void *) call, (void *) fn_name##_injection); \
    } \
    prefix ret fn_name##_injection args
#define HOOK_SINGLE_FROM_CALL(call, ret, name, args) _HOOK_SINGLE_FROM_CALL(call, ret, name, args, name, static)

#define _OVERWRITE_CALL(call, ret, name, args, prefix) \
    prefix ret name args; \
    __attribute__((constructor)) static void name##_hooking_init() { \
        overwrite_call((void *) call, (void *) name); \
    } \
    prefix ret name args
#define OVERWRITE_CALL(call, ret, name, args) _OVERWRITE_CALL(call, ret, name, args, static)

#define _OVERWRITE_CALLS(fn, ret, name, args, prefix) \
    prefix ret name args; \
    __attribute__((constructor)) static void name##_hooking_init() { \
        overwrite_calls((void *) fn, (void *) name); \
    } \
    prefix ret name args
#define OVERWRITE_CALLS(fn, ret, name, args) _OVERWRITE_CALLS(fn, ret, name, args, static)

#define UNUSED __attribute__((unused))

// Some decompiled symbols
extern "C" {
    CompoundTag *CompoundTag_getCompound_but_not(CompoundTag *self, const std::string &name);
}

// _non_virtual symbols I need to port
// Here's the script for it: `grep --color=no -rEo --no-filename '[a-zA-Z0-9_]+'_non_virtual src|sort -u`
// Then in python:
/*
e = """
...
"""
i=e.splitlines()
i=[j.replace('dup_','').replace('_vtable','')for j in i if j!='']
for j in i:
   print(f'printf("#define {j}_non_virtual = (({j}_t) %p)\\n", {j}_non_virtual);');
*/
#define Animal_findAttackTarget_non_virtual ((Animal_findAttackTarget_t) 0x85238)
#define DoorTile_use_non_virtual ((DoorTile_use_t) 0xbe2a8)
#define Entity_moveTo_non_virtual ((Entity_moveTo_t) 0x7a834)
#define GameMode_attack_non_virtual ((GameMode_attack_t) 0x1ac80)
#define ItemEntity_playerTouch_non_virtual ((ItemEntity_playerTouch_t) 0x86c58)
#define Level_getTile_non_virtual ((Level_getTile_t) 0xa3380)
#define PathfinderMob_updateAi_non_virtual ((PathfinderMob_updateAi_t) 0x83b3c)
#define PrimedTnt_interact_non_virtual ((PrimedTnt_interact_t) 0x7a980)
#define ServerLevel_tick_non_virtual ((ServerLevel_tick_t) 0x76a64)
#define Tile_addAABBs_non_virtual ((Tile_addAABBs_t) 0xc806c)
#define TileEntity_load_non_virtual ((TileEntity_load_t) 0xd21b4)
#define TileEntity_save_non_virtual ((TileEntity_save_t) 0xd25a0)
#define Tile_setShape_non_virtual ((Tile_setShape_t) 0xc2ce0)

// _non_virtual symbols I need to port
// Here's the script for it: `grep --color=no --no-filename -rEo 'dup_[^(]+' src|sort -u` and `grep --color=no --no-filename -Pro 'CUSTOM_VTABLE\([^ ]+ (\K[^)]+)\) {' src|cut -d")" -f1|sort -u`
// Then in python:
/*
e = """
...
"""
i=e.splitlines()
i=[j.replace('_non_virtual', '').replace('_vtable', '').replace('dup_', '') for j in i if j != '']
for j in i:
    print(f'#define dup_{j}_vtable(vtable) ({{{j}_vtable*obj=new {j}_vtable;memcpy((void*)obj,(void*)vtable,sizeof({j}_vtable));obj;}})')
*/
#define dup_EntityTile_vtable(vtable) ({EntityTile_vtable*obj=new EntityTile_vtable;memcpy((void*)obj,(void*)vtable,sizeof(EntityTile_vtable));obj;})
#define dup_Item_vtable(vtable) ({Item_vtable*obj=new Item_vtable;memcpy((void*)obj,(void*)vtable,sizeof(Item_vtable));obj;})
#define dup_Throwable_vtable(vtable) ({Throwable_vtable*obj=new Throwable_vtable;memcpy((void*)obj,(void*)vtable,sizeof(Throwable_vtable));obj;})
#define dup_Tile_vtable(vtable) ({Tile_vtable*obj=new Tile_vtable;memcpy((void*)obj,(void*)vtable,sizeof(Tile_vtable));obj;})
#define dup_DynamicTexture_vtable(vtable) ({DynamicTexture_vtable*obj=new DynamicTexture_vtable;memcpy((void*)obj,(void*)vtable,sizeof(DynamicTexture_vtable));obj;})
#define dup_Feature_vtable(vtable) ({Feature_vtable*obj=new Feature_vtable;memcpy((void*)obj,(void*)vtable,sizeof(Feature_vtable));obj;})
#define dup_TileEntity_vtable(vtable) ({TileEntity_vtable*obj=new TileEntity_vtable;memcpy((void*)obj,(void*)vtable,sizeof(TileEntity_vtable));obj;})
#define dup_TileEntityRenderer_vtable(vtable) ({TileEntityRenderer_vtable*obj=new TileEntityRenderer_vtable;memcpy((void*)obj,(void*)vtable,sizeof(TileEntityRenderer_vtable));obj;})

#define FISH
#ifdef FISH
// The most important function
bool is_fish();
#endif
