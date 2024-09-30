// Stores helper macros and symbols
#pragma once

#include <symbols/minecraft.h>
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
        overwrite_calls(fn, name); \
    } \
    prefix ret name args
#define OVERWRITE_CALLS(fn, ret, name, args) _OVERWRITE_CALLS(fn, ret, name, args, static)

#define UNUSED __attribute__((unused))

// Some decompiled symbols
extern "C" {
    CompoundTag *CompoundTag_getCompound_but_not(CompoundTag *self, const std::string &name);
}

//#define FISH
#ifdef FISH
// The most important function
bool is_fish();
#endif