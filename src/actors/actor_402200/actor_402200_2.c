#include "common.h"

#include "gameplay/3A34.h"

#include "actors/actor_402200.h"

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_2", func_actor_402200_80137D78);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_2", func_actor_402200_80137E48);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_2", func_actor_402200_80137EEC);

/// Tints the actor from the world position of its mesh root, then, while the
/// work block's 0x6EA state is 1 or 2, sets the display object's translate to
/// the matching shade and consumes the state.
void func_actor_402200_80137FB0(Actor402200* arg0)
{
    Actor402200Work* work;
    GpObj38*         obj;
    VECTOR           vec;
    s16              r;
    s16              g;
    s16              b;

    obj    = (GpObj38*)arg0->field_2C->field_8;
    work   = arg0->field_1C;
    vec.vx = obj->field_24.t[0];
    vec.vy = obj->field_24.t[1];
    vec.vz = obj->field_24.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
    switch (work->field_6EA) {
        case 1:
            r = 0;
            g = 0;
            b = 0x400;
            Gp_SetObjTrans((GpObj20*)arg0->field_2C, r, g, b);
            work->field_6EA = 0;
            break;
        case 2:
            r = 0xFFF;
            g = 0xFFF;
            b = 0xFFF;
            Gp_SetObjTrans((GpObj20*)arg0->field_2C, r, g, b);
            work->field_6EA = 0;
            break;
        case 0:
        default:
            return;
    }
}
