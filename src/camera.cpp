#include <symbols/minecraft.h>
#include <libreborn/libreborn.h>

#include "init.h"
#include "api.h"

// Camera control
float pitch = 0, yaw = 0;
static bool TripodCamera_hurt_injection(Mob *self, Entity *attacker, UNUSED int damage) {
    if (attacker != (Entity *) mc->player) return false;
    if (mc->player->input->is_sneaking) {
        // Remove
        self->vtable->remove(self);
        return false;
    }
    mc->camera = self;
    pitch = mc->player->pitch;
    yaw = mc->player->yaw;
    return false;
}
Mob_tick_t TripodCamera_tick_original = NULL;
static void TripodCamera_tick_injection(Mob *self) {
    if (self == mc->camera && mc->player) {
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
    // Call orignal
    TripodCamera_tick_original(self);
    // Check if removed
    if (self == mc->camera && self->pending_removal) {
        mc->camera = (Mob *) mc->player;
    }
}

// Camera legs texture fix
OVERWRITE_CALL(0x659dc, void, EntityRenderer_bindTexture_injection, (EntityRenderer *self, UNUSED std::string *file)) {
    std::string camera = "item/camera.png";
    EntityRenderer_bindTexture(self, &camera);
}
OVERWRITE_CALL(0x65a08, void, render_camera_legs, ()) {
    Tesselator *t = &Tesselator_instance;
    Tesselator_vertexUV(t, -.45, 0.5, -.45, 0.75, 0.5);
    Tesselator_vertexUV(t, -.45, -.5, -.45, 0.75, 1  );
    Tesselator_vertexUV(t, 0.45, -.5, 0.45, 1,    1  );
    Tesselator_vertexUV(t, 0.45, 0.5, 0.45, 1,    0.5);

    Tesselator_vertexUV(t, 0.45, 0.5, 0.45, 0.75, 0.5);
    Tesselator_vertexUV(t, 0.45, -.5, 0.45, 0.75, 1  );
    Tesselator_vertexUV(t, -.45, -.5, -.45, 1,    1  );
    Tesselator_vertexUV(t, -.45, 0.5, -.45, 1,    0.5);

    Tesselator_vertexUV(t, -.45, 0.5, 0.45, 0.75, 0.5);
    Tesselator_vertexUV(t, -.45, -.5, 0.45, 0.75, 1  );
    Tesselator_vertexUV(t, 0.45, -.5, -.45, 1,    1  );
    Tesselator_vertexUV(t, 0.45, 0.5, -.45, 1,    0.5);

    Tesselator_vertexUV(t, 0.45, 0.5, -.45, 0.75, 0.5);
    Tesselator_vertexUV(t, 0.45, -.5, -.45, 0.75, 1  );
    Tesselator_vertexUV(t, -.45, -.5, 0.45, 1,    1  );
    Tesselator_vertexUV(t, -.45, 0.5, 0.45, 1,    0.5);
}

__attribute__((constructor)) static void init() {
    // TODO: Fix astral projection bug
    // Camera preview
    patch_address((void *) 0x10c914, (void *) TripodCamera_hurt_injection);
    TripodCamera_tick_original = *(Mob_tick_t *) 0x10c8a4;
    patch_address((void *) 0x10c8a4, (void *) TripodCamera_tick_injection);
}
