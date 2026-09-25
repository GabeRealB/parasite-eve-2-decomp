#include "common.h"

#include "actors/actor_105300.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

extern u8 D_801153F4;

/// Tick handler of the main task (its state 1), switched on the gameplay mode
/// `D_801153F4`. Mode 1 only updates the colour; mode 2 sets the model's
/// `field_C` to 0x80 and the lock-on node's flags to 1 and stops there. Any
/// other mode runs the frame - mode 0 first clearing the model's `field_C` and
/// setting the node's flags to 8: the hit handler, the idle schedule, the pose
/// tick, the model's coordinate refresh, the colour update and the
/// regeneration step.
void func_actor_105300_80133468(GpEnemy* arg0, Actor05300* arg1)
{
    GsCOORDINATE2*   temp_s1;
    Actor05300Obj2C* temp_a1;
    s32              state;
    s32              one;

    temp_a1 = arg1->field_2C;
    temp_s1 = temp_a1->field_8;
    state   = D_801153F4;
    one     = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    temp_a1->field_C = 0;
    arg0->node.flags = 8;
    goto default_body;
case1:
    func_actor_105300_801335B8(arg1);
    return;
case2:
    temp_a1->field_C = 0x80;
    arg0->node.flags = one;
    return;
default_body:
    func_actor_105300_80131E3C(arg1);
    func_actor_105300_8013222C(arg1);
    func_actor_105300_80133610(arg1);
    temp_s1->flg = 0;
    Gp_UpdateCoord(temp_s1);
    func_actor_105300_801335B8(arg1);
    func_actor_105300_80133530(arg1);
}

/// Regeneration step: while the part object is alive (`field_336` clear)
/// and the enemy's hit points are below the ceiling `field_33C`, one point
/// comes back every five frames (`field_33E` counts them down), reported
/// through the lock-on node as a damage of -1.
void func_actor_105300_80133530(Actor05300* arg0)
{
    Actor05300Work* work;
    GpEnemy*        enemy;
    s16             timer;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if ((work->field_336 == 0) && (enemy->hp < work->field_33C)) {
        timer           = work->field_33E - 1;
        work->field_33E = timer;
        if ((timer << 0x10) <= 0) {
            enemy->hp = enemy->hp + 1;
            func_800DA6E8(&enemy->node, -1, 0);
            work->field_33E = 5;
        }
    }
}
