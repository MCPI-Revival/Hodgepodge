// Stores helper macros and symbols
#pragma once

#include <symbols/minecraft.h>
#include <libreborn/patch.h>
#include <cstring>

/*
#define _HOOK_FROM_CALL(call, ret, name, args, fn_name, prefix) \
    prefix name##_t fn_name##_original_FG6_API = NULL; \
    prefix ret fn_name##_injection args; \
    __attribute__((constructor)) static void fn_name##_hooking_init() { \
        fn_name##_original_FG6_API = (name##_t) extract_from_bl_instruction((uchar *) call); \
        overwrite_calls_manual((void *) fn_name##_original_FG6_API, (void *) fn_name##_injection); \
    } \
    prefix ret fn_name##_injection args
#define HOOK_FROM_CALL(call, ret, name, args) _HOOK_FROM_CALL(call, ret, name, args, name, static)
*/

#define _HOOK_SINGLE_FROM_CALL(call, ret, name, args, fn_name, prefix) \
    prefix name##_t fn_name##_original_FG6_API = NULL; \
    prefix ret fn_name##_injection args; \
    __attribute__((constructor)) static void fn_name##_hooking_init() { \
        fn_name##_original_FG6_API = (name##_t) extract_from_bl_instruction((uchar *) call); \
        overwrite_call_manual((void *) call, (void *) fn_name##_injection); \
    } \
    prefix ret fn_name##_injection args
#define HOOK_SINGLE_FROM_CALL(call, ret, name, args) _HOOK_SINGLE_FROM_CALL(call, ret, name, args, name, static)

#define _OVERWRITE_CALL_M(call, ret, name, args, prefix) \
    prefix ret name args; \
    __attribute__((constructor)) static void name##_hooking_init() { \
        overwrite_call_manual((void *) call, (void *) name); \
    } \
    prefix ret name args
#define OVERWRITE_CALL_M(call, ret, name, args) _OVERWRITE_CALL_M(call, ret, name, args, static)

#define _OVERWRITE_CALL(call, ret, name, args, prefix) \
    prefix ret name args; \
    __attribute__((constructor)) static void name##_hooking_init() { \
        overwrite_call(call, name); \
    } \
    prefix ret name args
#define OVERWRITE_CALL(call, ret, name, args) _OVERWRITE_CALL(call, ret, name, args, static)

#define _OVERWRITE_CALLS(fn, ret, name, args, prefix) \
    prefix ret name args; \
    __attribute__((constructor)) static void name##_hooking_init() { \
        overwrite_calls(fn, name); \
    } \
    prefix ret name args
#define OVERWRITE_CALLS(fn, ret, name, args) _OVERWRITE_CALLS(fn, ret, name, args, static)

#define UNUSED __attribute__((unused))

// Some decompiled symbols
CompoundTag *CompoundTag_getCompound_but_not(CompoundTag *self, const std::string &name);

// Misc Symbols
static Item::VTable *WeaponItem_vtable = (Item::VTable *) 0x10ef30;
static Item::VTable *ShovelItem_vtable = (Item::VTable *) 0x10f960;

typedef std::remove_pointer_t<decltype(Tile_getTexture2)>::ptr_type basic_Tile_getTexture2_t;
static basic_Tile_getTexture2_t ClothTile_getTexture2 = (basic_Tile_getTexture2_t) 0xca22c;
