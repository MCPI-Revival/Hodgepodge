#include <cmath>
#include <GLES/gl.h>

#include "api.h"
#include "minecarts.h"

#include <symbols/Entity.h>
#include <symbols/Player.h>
#include <symbols/Mob.h>
#include <symbols/Level.h>
#include <symbols/LevelSource.h>
#include <symbols/Tile.h>
#include <symbols/Material.h>
#include <symbols/MobRenderer.h>
#include <symbols/HumanoidMobRenderer.h>
#include <symbols/HumanoidModel.h>
#include <symbols/EntityRenderer.h>
#include <symbols/EntityRenderDispatcher.h>
#include <symbols/Model.h>
#include <symbols/ModelPart.h>
#include <libreborn/util/util.h>

/// Minecarts ///
std::unordered_map<Entity*, Minecart*> ridermap = {};
std::unordered_map<Entity*, std::pair<float, float>> riderrotmap = {};
Minecart *getRiding(Entity *entity) {
    if (!ridermap.contains(entity)) return NULL;
    return ridermap[entity];
}
void setRider(Minecart *vehicle, Entity *entity) {
    if (vehicle->rider != NULL) {
        // Eject rider
        vehicle->rider->moveTo(
            vehicle->self->x,
            vehicle->self->hitbox.y1,
            vehicle->self->z,
            vehicle->rider->yaw,
            vehicle->rider->pitch
        );
        ridermap.erase(vehicle->rider);
        riderrotmap.erase(vehicle->rider);
    }
    vehicle->rider = entity;
    if (entity == NULL) return;
    ridermap[entity] = vehicle;
    riderrotmap[entity] = {0.f, 0.f};
}

Minecart::Minecart(Level *level, float x, float y, float z) : CustomEntity(level) {
    self->renderer_id = 94;

    self->hitbox_width = 0.98f;
    self->hitbox_height = 0.7f;
    self->height_offset = self->hitbox_height / 2.0f;
    self->hitbox.x2 = self->hitbox.x1 + self->hitbox_width;
    self->hitbox.y2 = self->hitbox.y1 + self->hitbox_height;
    self->hitbox.z2 = self->hitbox.z1 + self->hitbox_width;

    self->velocity_x = 0;
    self->velocity_y = 0;
    self->velocity_z = 0;

    self->old_x = x;
    self->old_y = y;
    self->old_z = z;

    // TODO: Minecart sounds
    self->step_sounds = false;
    self->can_place_inside = true;

    moveTo(x, y + self->height_offset, z, 0, 0);
}

int Minecart::getEntityTypeId() const {
    return 94;
}

void Minecart::readAdditionalSaveData(CompoundTag *tag) {
    //INFO("Unimpl Minecart::readAdditionalSaveData");
}

void Minecart::addAdditonalSaveData(CompoundTag *tag) {
    //INFO("Unimpl Minecart::addAdditonalSaveData");
}

constexpr inline float normalizeAngle(float a) {
    if (a >= 180) a -= 360;
    if (a < -180) a += 360;
    return a;
}
void moveRider(Minecart *cart, Entity *rider) {
    float yo = rider->height_offset - 0.3f;
    if (!rider->isPlayer()) {
        rider->old_x = cart->self->old_x;
        rider->old_y = cart->self->old_y + yo;
        rider->old_z = cart->self->old_z;
    } else {
        yo -= 0.5f;
    }

    rider->setPos(cart->self->x, cart->self->y + yo, cart->self->z);
}
void Minecart::tick() {
    if (hurt_time > 0) hurt_time--;
    if (self->y < -64) self->outOfWorld();
    if (rider != NULL) {
        if (rider->pending_removal || rider->isSneaking()) {
            setRider(this, NULL);
        }
    }

    float _xo = self->x, _yo = self->y, _zo = self->z;
    float _pitch = self->pitch, _yaw = self->yaw;

    getalilmotion();
    getalilmotion();

    self->old_x = _xo;
    self->old_y = _yo;
    self->old_z = _zo;
    self->old_yaw = _yaw;
    self->old_pitch = _pitch;

    if (rider != NULL) moveRider(this, rider);
}

int isRail(Level *level, int x, int y, int z) {
    int ret = level->getTile(x, y, z);
    if (ret == RAIL_ID || ret == POWERED_RAIL_ID) {
        return ret;
    }
    return 0;
}

RailDir raildirs[11] = {
    RailDir{ 0,  0, -1,  0,  0,  1}, // 0: Z
    RailDir{-1,  0,  0,  1,  0,  0}, // 1: X
    RailDir{-1, -1,  0,  1,  0,  0}, // 2: X Up
    RailDir{-1,  0,  0,  1, -1,  0}, // 3: X Down
    RailDir{ 0,  0, -1,  0, -1,  1}, // 4: Z Up
    RailDir{ 0, -1, -1,  0,  0,  1}, // 5: Z Down
    RailDir{ 0,  0,  1,  1,  0,  0}, // 6: ZX
    RailDir{ 0,  0,  1, -1,  0,  0}, // 7: Zx
    RailDir{ 0,  0, -1, -1,  0,  0}, // 8: zx
    RailDir{ 0,  0, -1,  1,  0,  0}, // 9: zX
    // Invalid
    RailDir{0, 0, 0, 0, 0, 0} // 10
};
RailDir getRailDir(int data) {
    if (data < 0 || data > 9) return raildirs[10];
    return raildirs[data];
}

Vec3 *getPos(Entity *self, float x, float y, float z) {
    int xt = floor(x), yt = floor(y), zt = floor(z);
    if (isRail(self->level, xt, yt - 1, zt) != 0) yt--;

    int rail = isRail(self->level, xt, yt, zt);
    if (!rail) return NULL;
    int data = self->level->getData(xt, yt, zt);
    if (rail != RAIL_ID) data &= 0b111;
    RailDir direction = getRailDir(data);

    y = yt;
    if (direction.slope) y = yt + 1;

    float progress = 0;
    float x0 = xt + 0.5 + direction.x1 * 0.5;
    float y0 = yt + 0.5 + direction.y1 * 0.5;
    float z0 = zt + 0.5 + direction.z1 * 0.5;
    float x1 = xt + 0.5 + direction.x2 * 0.5;
    float y1 = yt + 0.5 + direction.y2 * 0.5;
    float z1 = zt + 0.5 + direction.z2 * 0.5;
    float xD = x1 - x0, zD = z1 - z0;
    float yD = (y1 - y0) * 2;

    if (xD == 0) {
        progress = z - zt;
    } else if (zD == 0) {
        progress = x - xt;
    } else {
        float xx = x - x0;
        float zz = z - z0;

        progress = (xx * xD + zz * zD) * 2;
    }

    x = x0 + xD * progress;
    y = y0 + yD * progress;
    z = z0 + zD * progress;
    if (yD < 0) y += 1;
    if (yD > 0) y += 0.5;
    return new Vec3{x, y, z};
}

Vec3 *getPosOffset(Entity *self, float x, float y, float z, float m) {
    int xt = floor(x), yt = floor(y), zt = floor(z);
    if (isRail(self->level, xt, yt - 1, zt) != 0) yt--;

    int rail = isRail(self->level, xt, yt, zt);
    if (!rail) return NULL;
    int data = self->level->getData(xt, yt, zt);
    if (rail != RAIL_ID) data &= 0b111;
    RailDir direction = getRailDir(data);

    y = yt;
    if (direction.slope) y = yt + 1;

    float xD = direction.x2 - direction.x1;
    float zD = direction.z2 - direction.z1;

    float mag = sqrt(xD * xD + zD * zD);
    if (mag > 0.0001) {
        x += (xD / mag) * m;
        z += (zD / mag) * m;
    }

    if (direction.y1 != 0 && floor(x) - xt == direction.x1 && floor(z) - zt == direction.z1)
        y += direction.y1;
    else if (direction.y2 != 0 && floor(x) - xt == direction.x2 && floor(z) - zt == direction.z2)
        y += direction.y2;

    return getPos(self, x, y, z);
}

void Minecart::getalilmotion() {
    self->old_x = self->x;
    self->old_y = self->y;
    self->old_z = self->z;

    self->velocity_y -= 0.04f;

    int xt = floor(self->x), yt = floor(self->y), zt = floor(self->z);
    if (isRail(self->level, xt, yt - 1, zt) != 0) yt--;

    float slideSpeed = 1 / 128.f;
    int rail = isRail(self->level, xt, yt, zt);
    if (rail) {
        Vec3 *oldPos = getPos(self, self->x, self->y, self->z);
        int data = self->level->getData(xt, yt, zt);
        self->y = yt;

        bool powered = false, unpowered = false;
        if (rail != RAIL_ID) {
            powered = (data & 0b1000) != 0;
            unpowered = !powered;
            data &= 0b111;
        }
        RailDir direction = getRailDir(data);

        if (direction.slope) self->y = yt + 1;
        if (data == 2) self->velocity_x -= slideSpeed;
        if (data == 3) self->velocity_x += slideSpeed;
        if (data == 4) self->velocity_z += slideSpeed;
        if (data == 5) self->velocity_z -= slideSpeed;

        float xD = direction.x2 - direction.x1;
        float zD = direction.z2 - direction.z1;
        float dd = sqrt(xD * xD + zD * zD);
        float flip = self->velocity_x * xD + self->velocity_z * zD;
        if (flip < 0) {
            xD = -xD;
            zD = -zD;
        }

        float pow = sqrt(self->velocity_x * self->velocity_x + self->velocity_z * self->velocity_z);
        if (dd > 0.001) {
            self->velocity_x = pow * xD / dd;
            self->velocity_z = pow * zD / dd;
        }

        if (unpowered) {
            float speedLength = sqrt(self->velocity_x * self->velocity_x + self->velocity_z * self->velocity_z);
            if (speedLength < 0.03) {
                self->velocity_x = 0;
                self->velocity_z = 0;
            } else {
                self->velocity_x /= 2.f;
                self->velocity_z /= 2.f;
            }
            self->velocity_y = 0;
        }

        float progress = 0;
        float x0 = xt + 0.5 + direction.x1 * 0.5;
        float z0 = zt + 0.5 + direction.z1 * 0.5;
        float x1 = xt + 0.5 + direction.x2 * 0.5;
        float z1 = zt + 0.5 + direction.z2 * 0.5;
        xD = x1 - x0;
        zD = z1 - z0;

        if (xD == 0) {
            progress = self->z - zt;
        } else if (zD == 0) {
            progress = self->x - xt;
        } else {
            float xx = self->x - x0;
            float zz = self->z - z0;
            progress = (xx * xD + zz * zD) * 2;
        }
        self->x = x0 + xD * progress;
        self->z = z0 + zD * progress;

        setPos(self->x, self->y + self->height_offset, self->z);

        float _xd = self->velocity_x;
        float _zd = self->velocity_z;
        if (rider != NULL) {
            _xd *= 0.75;
            _zd *= 0.75;
        }
        if (_xd < -TOP_CART_SPEED) _xd = -TOP_CART_SPEED;
        if (_xd > +TOP_CART_SPEED) _xd = +TOP_CART_SPEED;
        if (_zd < -TOP_CART_SPEED) _zd = -TOP_CART_SPEED;
        if (_zd > +TOP_CART_SPEED) _zd = +TOP_CART_SPEED;
        move(_xd, 0, _zd);

        if (direction.y1 != 0 && floor(self->x) - xt == direction.x1 && floor(self->z) - zt == direction.z1) {
            setPos(self->x, self->y + direction.y1, self->z);
        } else if (direction.y2 != 0 && floor(self->x) - xt == direction.x2 && floor(self->z) - zt == direction.z2) {
            setPos(self->x, self->y + direction.y2, self->z);
        }

        self->velocity_y = 0;
        if (rider != NULL) {
            self->velocity_x *= 0.997f;
            self->velocity_z *= 0.997f;
        } else {
            self->velocity_x *= 0.9f;
            self->velocity_z *= 0.9f;
        }

        Vec3 *newPos = getPos(self, self->x, self->y, self->z);
        if (newPos != NULL && oldPos != NULL) {
            float speed = (oldPos->y - newPos->y) * 0.05;

            pow = sqrt(self->velocity_x * self->velocity_x + self->velocity_z * self->velocity_z);
            if (pow > 0) {
                self->velocity_x = self->velocity_x / pow * (pow + speed);
                self->velocity_z = self->velocity_z / pow * (pow + speed);
            }
            setPos(self->x, newPos->y, self->z);
        }
        delete oldPos;
        delete newPos;

        int xn = floor(self->x);
        int zn = floor(self->z);
        if (xn != xt || zn != zt) {
            pow = sqrt(self->velocity_x * self->velocity_x + self->velocity_z * self->velocity_z);

            self->velocity_x = pow * (float)(xn - xt);
            self->velocity_z = pow * (float)(zn - zt);
        }

        // On a powered rail
        if (powered) {
            float speedLength = sqrt(self->velocity_x * self->velocity_x + self->velocity_z * self->velocity_z);
            if (speedLength > .01) {
                float speed = 0.06;
                self->velocity_x += (self->velocity_x / speedLength) * speed;
                self->velocity_z += (self->velocity_z / speedLength) * speed;
            } else if (data == 1) {
                if (self->level->isSolidBlockingTile(xt - 1, yt, zt)) {
                    self->velocity_x = .02;
                } else if (self->level->isSolidBlockingTile(xt + 1, yt, zt)) {
                    self->velocity_x = -.02;
                }
            } else if (data == 0) {
                if (self->level->isSolidBlockingTile(xt, yt, zt - 1)) {
                    self->velocity_z = .02;
                } else if (self->level->isSolidBlockingTile(xt, yt, zt + 1)) {
                    self->velocity_z = -.02;
                }
            }
        }
    } else {
        if (self->velocity_x < -TOP_CART_SPEED) self->velocity_x = -TOP_CART_SPEED;
        if (self->velocity_x > +TOP_CART_SPEED) self->velocity_x = +TOP_CART_SPEED;
        if (self->velocity_z < -TOP_CART_SPEED) self->velocity_z = -TOP_CART_SPEED;
        if (self->velocity_z > +TOP_CART_SPEED) self->velocity_z = +TOP_CART_SPEED;

        if (self->on_ground) {
            self->velocity_x *= 0.5f;
            self->velocity_y *= 0.5f;
            self->velocity_z *= 0.5f;
        }
        self->move(self->velocity_x, self->velocity_y, self->velocity_z);

        if (!self->on_ground) {
            self->velocity_x *= 0.95f;
            self->velocity_y *= 0.95f;
            self->velocity_z *= 0.95f;
        }
    }

    self->pitch = 0;
    float xDiff = self->old_x - self->x;
    float zDiff = self->old_z - self->z;
    if (xDiff * xDiff + zDiff * zDiff > 0.001) {
        self->yaw = (float) (atan2(zDiff, xDiff) * 180 / M_PI);
        if (flipped) self->yaw += 180;
    }

    float rotDiff = normalizeAngle(self->yaw - self->old_yaw);
    if (rotDiff < -170 || rotDiff >= 170) {
        self->yaw += 180;
        flipped = !flipped;
    }
    setRot(self->yaw, self->pitch);

    if (!self->level->is_client_side) {
        AABB chunk = {
            self->hitbox.x1 - 0.2f,
            self->hitbox.y1,
            self->hitbox.z1 - 0.2f,
            self->hitbox.x2 + 0.2f,
            self->hitbox.y2,
            self->hitbox.z2 + 0.2f,
        };
        std::vector<Entity*> *entities = self->level->getEntities(self, chunk);
        if (entities) {
            for (Entity *i : *entities) {
                if (i != rider && i->isPushable() && i->getEntityTypeId() == self->getEntityTypeId()) {
                    i->push(self);
                }
            }
        }
    }
}

void Minecart::push(Entity *e) {
    if (self->level->is_client_side) return;
    if (e == rider) return;
    if (e->isAlive() && !e->isPlayer() && !e->isSneaking()) {
        if (rider == NULL && getRiding(e) == NULL) {
            setRider(this, e);
        }
    }

    float xa = e->x - self->x;
    float za = e->z - self->z;
    float distSqr = xa * xa + za * za;
    if (distSqr >= 0.0001) {
        float distance = sqrt(distSqr);
        if (distance > 0) {
            xa /= distance;
            za /= distance;

            float pw = 1.0 / distance;
            if (pw > 1.0) pw = 1.0;
            xa *= pw * 0.05;
            za *= pw * 0.05;
        }

        if (e->getEntityTypeId() == self->getEntityTypeId()) {
            float avgx = (e->velocity_x + self->velocity_x) / 2.0;
            float avgz = (e->velocity_z + self->velocity_z) / 2.0;
            self->velocity_x *= 0.2;
            self->velocity_z *= 0.2;
            self->push2(avgx - xa, 0.0, avgz - za);
            e->velocity_x *= 0.2;
            e->velocity_z *= 0.2;
            e->push2(avgx + xa, 0.0, avgz + za);
        } else {
            self->push2(-xa, 0.0, -za);
            e->push2(xa / 4.0, 0.0, za / 4.0);
        }
    }
}

bool Minecart::interact(Player *with) {
    if (rider == NULL) setRider(this, (Entity *) with);
    return true;
}

bool Minecart::hurt(Entity *attacker, int damage) {
    self->remove();
    setRider(this, NULL);
    if (!attacker->isPlayer() || ((Player *) attacker)->abilities.infinite_items) {
        // Spawn item
        summon_item(self->level, self->x + 0.5f, self->y, self->z + 0.5f, ItemInstance{
            .count = 1, .id = MINECART_ITEM_ID, .auxiliary = 0
        });
    }
    return false;
}


/// Minecart Riding ///
OVERWRITE_CALL_M(
    0xa4cf4, /* Level::tick's call of entity->tick() */
    void, Entity_tickInCart, (Entity *self)
) {
    Minecart *riding = getRiding(self);
    if (riding && riding->self->pending_removal) {
        setRider(riding, NULL);
        riding = NULL;
    }
    if (riding == NULL) {
        // Only call orignal
        self->tick();
        return;
    }

    self->fall_distance = 0;
    self->velocity_x = 0;
    self->velocity_y = 0;
    self->velocity_z = 0;

    // Call orignal
    self->tick();

    if (self->isMob()) {
        ((Mob *) self)->old_walking_animation = 0.0;
        ((Mob *) self)->walking_animation = 0.0;
        ((Mob *) self)->walking_animation_frame = 0.0;
    }

    riding = getRiding(self);
    if (riding == NULL) return;

    moveRider(riding, self);

    // Smoothly move rotation as well
    auto riderot = riderrotmap[self];
    riderot.first += riding->self->yaw - riding->self->old_yaw;
    riderot.second += riding->self->pitch - riding->self->old_pitch;

    riderot.first = normalizeAngle(riderot.first);
    riderot.second = normalizeAngle(riderot.second);
    float yd = std::min(std::max(riderot.first/2.f, -10.f), 10.f);
    float pd = std::min(std::max(riderot.second/2.f, -10.f), 10.f);

    riderot.first -= yd;
    riderot.second -= pd;
    if (!self->isPlayer()) {
        self->yaw += yd;
        self->pitch += pd;
    }

    riderrotmap[self] = riderot;
}

// Disable self-pushing when riding
OVERWRITE_CALLS(
    Entity_push,
    void, Entity_push_injection, (Entity_push_t original, Entity *self, Entity *entity)
) {
    Minecart *riding = getRiding(self);
    if (riding && riding->self == entity) return;
    // Call orignal
    original(self, entity);
}

// Disable step sounds when riding
OVERWRITE_CALLS(
    Entity_playStepSound,
    void, Entity_playStepSound_injection, (Entity_playStepSound_t original, Entity *self, int x, int y, int z, int t)
) {
    // Call orignal
    if (getRiding(self) == NULL) original(self, x, y, z, t);
}

/// Minecart rendering ///
struct MinecartModel : CustomModel {
    ModelPart *cubes[6];

    MinecartModel() : CustomModel() {
        self->attackTime = 0;
        self->riding = false;
        self->textureWidth = 64;
        self->textureHeight = 32;
        self->baby = false;

        cubes[0] = (ModelPart *) ::operator new(sizeof(ModelPart));
        ModelPart_constructor->get(false)(cubes[0], (Model *) self, 0, 10);
        cubes[1] = (ModelPart *) ::operator new(sizeof(ModelPart));
        ModelPart_constructor->get(false)(cubes[1], (Model *) self, 0, 0);
        cubes[2] = (ModelPart *) ::operator new(sizeof(ModelPart));
        ModelPart_constructor->get(false)(cubes[2], (Model *) self, 0, 0);
        cubes[3] = (ModelPart *) ::operator new(sizeof(ModelPart));
        ModelPart_constructor->get(false)(cubes[3], (Model *) self, 0, 0);
        cubes[4] = (ModelPart *) ::operator new(sizeof(ModelPart));
        ModelPart_constructor->get(false)(cubes[4], (Model *) self, 0, 0);
        cubes[5] = (ModelPart *) ::operator new(sizeof(ModelPart));
        ModelPart_constructor->get(false)(cubes[5], (Model *) self, 44, 10);

        cubes[0]->addBox2(-10.f, -8.f, -1.f, 20, 16, 2, 0);
        cubes[0]->setPos(0, 4.f, 0);
        cubes[0]->xRot = M_PI / 2;

        cubes[1]->addBox2(-8.f, -9.f, -1.f, 16, 8, 2, 0);
        cubes[1]->setPos(-9, 4, 0);
        cubes[1]->yRot = M_PI / 2 * 3;

        cubes[2]->addBox2(-8.f, -9.f, -1.f, 16, 8, 2, 0);
        cubes[2]->setPos(9, 4, 0);
        cubes[2]->yRot = M_PI / 2;

        cubes[3]->addBox2(-8.f, -9.f, -1.f, 16, 8, 2, 0);
        cubes[3]->setPos(0, 4, -7);
        cubes[3]->yRot = M_PI / 2 * 2;

        cubes[4]->addBox2(-8.f, -9.f, -1.f, 16, 8, 2, 0);
        cubes[4]->setPos(0, 4, 7);

        cubes[5]->addBox2(-9.f, -7.f, -1.f, 18, 14, 1, 0);
        cubes[5]->setPos(0, 4, 0);
        cubes[5]->xRot = -M_PI / 2;
        cubes[5]->y = 4.1;
    }

    void render(Entity *entity, float time, float r, float bob, float yRot, float xRot, float scale) override {
        cubes[0]->render(scale);
        cubes[1]->render(scale);
        cubes[2]->render(scale);
        cubes[3]->render(scale);
        cubes[4]->render(scale);
        cubes[5]->render(scale);
    }

};

struct MinecartRenderer : CustomEntityRenderer {
    Model *model;

    MinecartRenderer() : CustomEntityRenderer() {
        self->shadow_radius = 0.5f;
        model = (Model *) (new MinecartModel())->self;
    };

    void render(Entity *cart, float x, float y, float z, float rot, float a) override {
        media_glPushMatrix();

        long seed = cart->id * 0x1d66f537l;
        seed *= seed * 0x105cb26d1l + 98761;

        // Range of [0.00175, -0.00175]
        float xo = ((seed >> 16) & 0b111)*0.0005f - 0.00175f;
        float yo = ((seed >> 20) & 0b111)*0.0005f - 0.00175f;
        float zo = ((seed >> 24) & 0b111)*0.0005f - 0.00175f;

        media_glTranslatef(xo, yo, zo);

        float xx    = cart->old_x     + (cart->x     - cart->old_x    ) * a;
        float yy    = cart->old_y     + (cart->y     - cart->old_y    ) * a;
        float zz    = cart->old_z     + (cart->z     - cart->old_z    ) * a;
        float pitch = cart->old_pitch + (cart->pitch - cart->old_pitch) * a;

        float r = 0.3f;
        Vec3 *p = getPos(cart, xx, yy, zz);
        if (p != NULL) {
            Vec3 *p1 = getPosOffset(cart, xx, yy, zz, r);
            if (p1 == NULL) p1 = p;
            Vec3 *p2 = getPosOffset(cart, xx, yy, zz, -r);
            if (p2 == NULL) p2 = p;

            x += p->x - xx;
            z += p->z - zz;
            y += (p1->y + p2->y) / 2 - yy;

            p2->x -= p1->x;
            p2->y -= p1->y;
            p2->z -= p1->z;
            float mag = sqrt(p2->x * p2->x + p2->y * p2->y + p2->z * p2->z);
            if (mag > 0.0001) {
                rot = atan2(p2->z / mag, p2->x / mag) * 180 / M_PI;
                pitch = atan(p2->y / mag) * 73;
            }
            if (p1 != p) delete p1;
            if (p2 != p) delete p2;
            delete p;
        }
        media_glTranslatef(x, y, z);

        media_glRotatef(180 - rot, 0, 1, 0);
        media_glRotatef(-pitch, 0, 0, 1);

        bindTexture("/item/cart.png");
        media_glScalef(-1, -1, 1);
        model->render(cart, 0, 0, 0, 0, 0, 1 / 16.0f);
        media_glPopMatrix();
    }
};

OVERWRITE_CALLS(
    EntityRenderDispatcher_constructor,
    EntityRenderDispatcher *, EntityRenderDispatcher_constructor_injection, (EntityRenderDispatcher_constructor_t original, EntityRenderDispatcher *self)
) {
    original(self);

    MinecartRenderer *renderer = new MinecartRenderer();
    self->assign(94, (EntityRenderer *) renderer->self);

    return self;
}

/// Minecart Rider Rendering ///
void HumanoidMobRenderer_render_injection_riding(
    HumanoidMobRenderer_render_t original, HumanoidMobRenderer *self, Entity *entity, float x, float y, float z, float rot, float a
) {
    Minecart *v = getRiding(entity);
    if (self->model) self->model->riding = v != NULL;
    if (self->armor) self->armor->riding = v != NULL;
    if (self->humanoid_model) self->humanoid_model->riding = v != NULL;

    original(self, entity, x, y, z, rot, a);
}
void MobRenderer_render_injection_riding(MobRenderer_render_t original, MobRenderer *self, Entity *entity, float x, float y, float z, float rot, float a) {
    Minecart *v = getRiding(entity);
    if (self->model) self->model->riding = v != NULL;
    if (self->armor) self->armor->riding = v != NULL;

    original(self, entity, x, y, z, rot, a);
}

__attribute__((constructor)) static void init_MobRenderer_render_injector() {
    typedef std::remove_pointer_t<decltype(MobRenderer_render)>::ptr_type func_t;
    void *HumanoidMobRenderer_render_MobRenderer_render_call = (void *) 0x62c00;
    func_t MobRenderer_render_prev = (func_t)
        extract_from_bl_instruction((unsigned char *) HumanoidMobRenderer_render_MobRenderer_render_call);
    overwrite_calls(MobRenderer_render, MobRenderer_render_injection_riding);
    overwrite_calls(HumanoidMobRenderer_render, HumanoidMobRenderer_render_injection_riding);
    overwrite_call_manual((void *) HumanoidMobRenderer_render_MobRenderer_render_call, (void *) MobRenderer_render_prev);
    unsigned char yet_another_nop_patch[4] = {0x00, 0xf0, 0x20, 0xe3}; // "nop"
    patch((void *) 0x64224, yet_another_nop_patch);
}

/// Rail Tiles ///
static Tile *powered_rail = NULL;
static Tile *rail = NULL;

struct Rail {
    Level *level;
    int x, y, z;
    std::vector<Pos> connected = {};
    bool canTurn = false;

    bool connects(Rail *rail) {
        for (Pos p : connected) {
            if (p.x == rail->x && /*p.y == rail->y &&*/ p.z == rail->z) {
                return true;
            }
        }
        return false;
    }
    bool canConnect(Rail *rail) {
        if (connects(rail)) return true;
        return connected.size() != 2;
    }

    void setConnections(int data) {
        if (canTurn) data &= 0b111;
        connected.clear();
        connected.push_back({x + raildirs[data].x1, y - raildirs[data].y2, z + raildirs[data].z1});
        connected.push_back({x + raildirs[data].x2, y - raildirs[data].y1, z + raildirs[data].z2});
    }

    // Can't be called by init, as it causes a loop
    Rail(Level *level, int x, int y, int z)
        : level(level), x(x), y(y), z(z)
    {
        canTurn = level->getTile(x, y, z) == RAIL_ID;
        setConnections(level->getData(x, y, z));
    }

    static Rail *getRail(Level *level, int x, int y, int z) {
        if (isRail(level, x, y, z)) return new Rail(level, x, y, z);
        if (isRail(level, x, y + 1, z)) return new Rail(level, x, y + 1, z);
        if (isRail(level, x, y - 1, z)) return new Rail(level, x, y - 1, z);
        return NULL;
    }

    void updateConnections() {
        for (int i = 0; i < connected.size(); i++) {
            Rail *rail = getRail(level, connected[i].x, connected[i].y, connected[i].z);
            if (rail == NULL || !rail->connects(this)) {
                connected.erase(connected.begin() + i--);
            } else {
                connected[i] = {rail->x, rail->y, rail->z};
            }
            delete rail;
        }
    }

    bool canRailConnect(int x, int y, int z) {
        Rail *rail = getRail(level, x, y, z);
        if (rail == NULL) return false;
        rail->updateConnections();
        bool ret = rail->canConnect(this);
        delete rail;
        return ret;
    }

    void connectTo(Rail *other) {
        connected.push_back({other->x, other->y, other->z});

        bool xp = canRailConnect(x + 1, y, z);
        bool xm = canRailConnect(x - 1, y, z);
        bool zp = canRailConnect(x, y, z + 1);
        bool zm = canRailConnect(x, y, z - 1);

        int dir = 10;
        if (zm || zp) dir = 0;
        if (xm || xp) dir = 1;
        if (canTurn) {
            if (zm && xp && !zp && !xm) dir = 9;
            if (zm && xm && !zp && !xp) dir = 8;
            if (zp && xm && !zm && !xp) dir = 7;
            if (zp && xp && !zm && !xm) dir = 6;
        }

        if (dir == 0) {
            if (isRail(level, x, y + 1, z - 1)) dir = 4;
            if (isRail(level, x, y + 1, z + 1)) dir = 5;
        }
        if (dir == 1) {
            if (isRail(level, x + 1, y + 1, z)) dir = 2;
            if (isRail(level, x - 1, y + 1, z)) dir = 3;
        }
        if (dir == 10) dir = 0;

        int data = dir;
        if (!canTurn) data |= level->getData(x, y, z) & 0b1000;
        level->setData(x, y, z, data);
    }

    void updateTile(bool withPower, bool forceUpdate) {
        bool xp = canRailConnect(x + 1, y, z);
        bool xm = canRailConnect(x - 1, y, z);
        bool zp = canRailConnect(x, y, z + 1);
        bool zm = canRailConnect(x, y, z - 1);

        int dir = 10;
        if ((zm || zp) && !xm && !xp) dir = 0;
        if ((xm || xp) && !zm && !zp) dir = 1;
        if (canTurn) {
            if (zp && xp && !zm && !xm) dir = 6;
            if (zp && xm && !zm && !xp) dir = 7;
            if (zm && xm && !zp && !xp) dir = 8;
            if (zm && xp && !zp && !xm) dir = 9;
        }
        if (dir == 10) {
            if (zm || zp) dir = 0;
            if (xm || xp) dir = 1;

            if (canTurn) {
                if (withPower) {
                    if (zp && xp) dir = 6;
                    if (xm && zp) dir = 7;
                    if (xp && zm) dir = 9;
                    if (zm && xm) dir = 8;
                } else {
                    if (zm && xm) dir = 8;
                    if (xp && zm) dir = 9;
                    if (xm && zp) dir = 7;
                    if (zp && xp) dir = 6;
                }
            }
        }
        if (dir == 0) {
            if (isRail(level, x, y + 1, z - 1)) dir = 4;
            if (isRail(level, x, y + 1, z + 1)) dir = 5;
        }
        if (dir == 1) {
            if (isRail(level, x + 1, y + 1, z)) dir = 2;
            if (isRail(level, x - 1, y + 1, z)) dir = 3;
        }
        if (dir == 10) dir = 0;

        setConnections(dir);

        int data = dir;
        int oldData = level->getData(x, y, z);
        if (!canTurn) data |= oldData & 0b1000;
        if (forceUpdate || data != oldData) {
            level->setData(x, y, z, data);
            for (Pos p : connected) {
                Rail *rail = getRail(level, p.x, p.y, p.z);
                if (rail == NULL) continue;
                rail->updateConnections();
                if (rail->canConnect(this)) {
                    rail->connectTo(this);
                }
            }
        }
    }

    int countAdjacentTracks() {
        int ret = 0;
        if (isRail(level, x, y, z - 1)) ret++;
        if (isRail(level, x, y, z + 1)) ret++;
        if (isRail(level, x - 1, y, z)) ret++;
        if (isRail(level, x + 1, y, z)) ret++;
        return ret;
    }
};


struct RailTile final : CustomTile {
    RailTile(const int id, const int texture, const Material *material)
        : CustomTile(id, texture, material) {}

    AABB *getAABB(UNUSED Level *level, UNUSED int x, UNUSED int y, UNUSED int z) override {
        return NULL;
    }
    bool isSolidRender() override {
        return false;
    }
    bool isCubeShaped() override {
        return false;
    }
    int getRenderShape() override {
        return 55;
    }
    void updateShape(LevelSource *level, int x, int y, int z) override {
        int data = level->getData(x, y, z);
        setShape(0, 0, 0, 1, 0.125 + (0.5f * (2 <= data && data <= 5)), 1);
    }

    int getTexture2(UNUSED int face, int data) {
        if (self->id == POWERED_RAIL_ID) {
            if ((data & 0b1000) == 0b1000) {
                return self->texture + 16;
            }
            return self->texture;
        }
        if (self->id == RAIL_ID && data >= 6) return self->texture - 16;
        return self->texture;
    }

    bool mayPlace(Level *level, int x, int y, int z, UNUSED unsigned char face) override {
        return level->isSolidBlockingTile(x, y - 1, z);
    }

    void onPlace(Level *level, int x, int y, int z) override {
        Rail *rail = Rail::getRail(level, x, y, z);
        if (rail) rail->updateTile(level->hasNeighborSignal(x, y, z), true);
        delete rail;
        if (self->id == POWERED_RAIL_ID) neighborChanged(level, x, y, z, POWERED_RAIL_ID);
    }

    static bool connectedToPoweredRail2(Level *level, int x, int y, int z, bool forward, int axis, int d) {
        if (level->getTile(x, y, z) != POWERED_RAIL_ID) return false;
        int data = level->getData(x, y, z);
        if ((data & 0b1000) != 0b1000) return false;

        // Perpendicular rails
        data &= 0b111;
        if (axis == 0 && (data == 0 || data == 4 || data == 5)) return false;
        if (axis == 1 && (data == 1 || data == 2 || data == 3)) return false;

        return level->hasNeighborSignal(x, y, z)
            || level->hasNeighborSignal(x, y + 1, z)
            || connectedToPoweredRail(level, x, y, z, forward, d + 1);
    }

    static bool connectedToPoweredRail(Level *level, int x, int y, int z, bool forward, int d) {
        if (d >= 8) return false;

        int data = level->getData(x, y, z) & 0b111;
        RailDir dir = getRailDir(data);
        if (forward) {
            x += dir.x1;
            y += -dir.y2;
            z += dir.z1;
        } else {
            x += dir.x2;
            y += -dir.y1;
            z += dir.z2;
        }
        int axis = -1;
        if (data == 1 || data == 2 || data == 3) axis = 0;
        if (data == 0 || data == 4 || data == 5) axis = 1;

        if (connectedToPoweredRail2(level, x, y, z, forward, axis, d)) return true;
        return connectedToPoweredRail2(level, x, y - 1, z, forward, axis, d);
    }

    void neighborChanged(Level *level, int x, int y, int z, int neighborId) override {
        if (level->is_client_side) return;

        int data = level->getData(x, y, z);
        int dir = data;
        if (self->id != RAIL_ID) dir &= 0b111;

        bool invalid = false;
        if (!level->isSolidBlockingTile(x, y - 1, z)) invalid = true;
        if (dir == 2 && !level->isSolidBlockingTile(x + 1, y, z)) invalid = true;
        if (dir == 3 && !level->isSolidBlockingTile(x - 1, y, z)) invalid = true;
        if (dir == 4 && !level->isSolidBlockingTile(x, y, z - 1)) invalid = true;
        if (dir == 5 && !level->isSolidBlockingTile(x, y, z + 1)) invalid = true;

        if (invalid) {
            level->setTile(x, y, z, 0);
            summon_item(level, x, y, z, ItemInstance{
                .count = 1, .id = self->id, .auxiliary = 0
            });
            return;
        }

        if (self->id == POWERED_RAIL_ID) {
            bool signal = level->hasNeighborSignal(x, y, z)
                || level->hasNeighborSignal(x, y + 1, z)
                || connectedToPoweredRail(level, x, y, z, true, 0)
                || connectedToPoweredRail(level, x, y, z, false, 0);

            bool updated = false;
            if (signal && (data & 0b1000) == 0) {
                // Turn on
                level->setData(x, y, z, data | 0b1000);
                updated = true;
            } else if (!signal && (data & 0b1000) != 0) {
                // Turn off
                level->setData(x, y, z, dir);
                updated = true;
            }

            if (updated) {
                level->updateNearbyTiles(x, y - 1, z, POWERED_RAIL_ID);
                if (getRailDir(dir).slope) {
                    level->updateNearbyTiles(x, y + 1, z, POWERED_RAIL_ID);
                }
            }
        } else if (
            (Tile::tiles[neighborId] == NULL || Tile::tiles[neighborId]->isSignalSource())
            && self->id == RAIL_ID
        ) {
            Rail *rail = Rail::getRail(level, x, y, z);
            if (rail && rail->countAdjacentTracks() == 3) {
                rail->updateTile(level->hasNeighborSignal(x, y, z), false);
            }
            delete rail;
        }
    }
};

void make_rails() {
    powered_rail = (new RailTile(POWERED_RAIL_ID, 163, Material::metal))->self;
    powered_rail->init();
    powered_rail->setDestroyTime(0.7f);
    powered_rail->category = 4;
    powered_rail->setDescriptionId("goldenRail");
    powered_rail->setShape(0.0f, 0.0f, 0.0f, 1.0f, 0.125f, 1.0f);

    rail = (new RailTile(RAIL_ID, 128, Material::metal))->self;
    rail->init();
    rail->setDestroyTime(0.7f);
    rail->category = 4;
    rail->setDescriptionId("rail");
    rail->setShape(0.0f, 0.0f, 0.0f, 1.0f, 0.125f, 1.0f);
}

/// Minecart Item ///
Item *minecart_item = NULL;
struct MinecartItem final : CustomItem {
    MinecartItem(int id) : CustomItem(id) {}

    bool useOn(ItemInstance *item_instance, Player *player, Level *level, int x, int y, int z, int hit_side, float hit_x, float hit_y, float hit_z) override {
        if (!isRail(level, x, y, z)) return false;
        Minecart *cart = new Minecart(level, x + 0.5f, y + 0.5f, z + 0.5f);
        level->addEntity((Entity *) cart->self);
        item_instance->count--;
        return true;
    }
};

void make_minecart_item() {
    minecart_item = (new MinecartItem(MINECART_ITEM_ID - 256))->self;

    std::string name = "minecart";
    minecart_item->setDescriptionId(name);
    minecart_item->max_stack_size = 1;
    minecart_item->category = 2;
    minecart_item->texture = 7 + 16*8;
}
