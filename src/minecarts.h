#include <symbols/Entity.h>

#define TOP_CART_SPEED 0.4
#define RAIL_ID 66
#define POWERED_RAIL_ID 27
#define DETECTOR_RAIL_ID 28
#define MINECART_ITEM_ID 328

struct RailDir {
    int x1, y1, z1, x2, y2, z2;

    bool straight, slope, turn;
    RailDir(int x1, int y1, int z1, int x2, int y2, int z2)
        : x1(x1), y1(y1), z1(z1), x2(x2), y2(y2), z2(z2)
    {
        straight = x1 == -x2 || z1 == -z2;
        slope = y1 != 0 || y2 != 0;
        turn = !slope && !straight;
    }
};
RailDir getRailDir(int data);

struct Minecart final : CustomEntity {
    Entity *rider = NULL;
    int hurt_time = 0;
    bool flipped = false;

    Minecart(Level *level, float x, float y, float z);
    int getEntityTypeId() const override;
    void readAdditionalSaveData(CompoundTag *tag) override;
    void addAdditonalSaveData(CompoundTag *tag) override;
    void tick() override;
    bool isPushable() override { return true; };
    bool isPickable() override { return true; };
    bool isAlive() override { return false; };
    void push(Entity *entity) override;
    bool interact(Player *with) override;
    bool hurt(Entity *attacker, int damage) override;

    void getalilmotion();
};

void make_minecart_item();
void make_rails();
