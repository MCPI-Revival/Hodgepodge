#include <symbols/minecraft.h>
#include <libreborn/libreborn.h>

#include "init.h"
#include "api.h"

// Camera control
float pitch = 0, yaw = 0;
static bool TripodCamera_hurt_injection(TripodCamera *self, Entity *attacker, UNUSED int damage) {
    if (attacker != (Entity *) mc->player) return false;
    if (mc->player->input->is_sneaking) {
        // Remove
        self->remove();
        return false;
    }
    mc->camera = (Mob *) self;
    pitch = mc->player->pitch;
    yaw = mc->player->yaw;
    return false;
}
static void TripodCamera_tick_injection(TripodCamera_tick_t original, TripodCamera *self) {
    if (((Mob *) self) == mc->camera && mc->player) {
        self->pitch += mc->player->pitch - pitch;
        self->yaw += mc->player->yaw - yaw;
        mc->player->pitch = pitch;
        mc->player->yaw = yaw;
        // Adjust rot
        if (self->pitch >= 90) self->pitch = 89;
        else if (self->pitch <= -90) self->pitch = -89;
        if (self->yaw > 360) self->yaw -= 360;
        else if (self->yaw < -360) self->yaw += 360;
        // Check for exits
        if (mc->player->input->is_sneaking) {
            // Release the camera
            mc->camera = (Mob *) mc->player;
        }
    }
    // Call original
    original(self);
    // Check if removed
    if (((Mob *) self) == mc->camera && self->pending_removal) {
        mc->camera = (Mob *) mc->player;
    }
}

__attribute__((constructor)) static void init() {
    // TODO: Fix astral projection bug
    // Camera preview
    patch_vtable(TripodCamera_hurt, TripodCamera_hurt_injection);
    overwrite_calls(TripodCamera_tick, TripodCamera_tick_injection);
}
