#include "common.h"

#include "actors/actor_101500.h"
#include "gameplay/gameplay.h"

extern u8 D_801153F4;

/// Per-frame handler. Scene mode 1 only recolours and shadows the actor and
/// mode 2 hides it; otherwise it applies pending hit reactions and contacts,
/// hands off to the teardown state once `field_378` is raised in the
/// states that allow it, runs the behaviour state, turns, moves, animates
/// and voices the actor and rebuilds its root coordinate.
void Actor01500_Fn02484(GpEnemy* arg0, Actor101500* arg1)
{
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    Actor101500Work* work;
    s32              state;
    s32              one;

    obj   = arg1->field_2C;
    state = D_801153F4;
    work  = arg1->field_1C;
    coord = obj->coords;
    one   = 1;
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
    obj->flags       = 0;
    arg0->node.flags = 0;
    goto default_body;
case2:
    obj->flags       = 0x80;
    arg0->node.flags = one;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor01500_Fn025C8(arg1);
    }
    Actor01500_Fn004EC(arg1);
    if (work->field_378 != 0) {
        if ((work->field_35A == 5) || (work->field_37E != 0)) {
            work->field_35A = 8;
            work->field_35C = 0;
            arg1->field_30  = 2;
        }
    }
    Actor01500_Fn026D8(arg1);
    if (work->field_376 != 0) {
        Actor01500_Fn01838(arg1);
    }
    Actor01500_Fn01988(arg1);
    Actor01500_Fn02958(arg1);
    Actor01500_Fn02A1C(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor01500_Fn02B14(arg1);
    Actor01500_Fn02B70(arg1);
}
