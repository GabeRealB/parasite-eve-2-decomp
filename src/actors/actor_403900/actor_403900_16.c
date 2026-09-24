#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"

#include "actors/actor_403900.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Relights the actor from its root part's world position and consumes a
/// pending `field_6EA` tint request: 1 hands the display object the translate
/// (0, 0, 0x400), 2 the full (0xFFF, 0xFFF, 0xFFF), and either clears the
/// request.
void func_actor_403900_80137FB4(Actor403900* arg0)
{
    Actor403900Work* work;
    GsCOORDINATE2*   obj;
    VECTOR           vec;
    s16              r;
    s16              g;
    s16              b;

    obj    = (GsCOORDINATE2*)arg0->field_2C->field_8;
    work   = arg0->field_1C;
    vec.vx = obj->workm.t[0];
    vec.vy = obj->workm.t[1];
    vec.vz = obj->workm.t[2];
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

/// Draws the ground shadow quad, 0x300 across, under the fourth part's
/// horizontal position at the root's height, shaded by `field_6E2` - which a
/// zero turns into -1 first, so a shadow nothing has raised is not drawn.
void func_actor_403900_80138070(Actor403900* arg0)
{
    Actor403900Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   sub;
    VECTOR3          vec;

    work  = arg0->field_1C;
    coord = &arg0->field_2C->field_8->field_0;
    sub   = &arg0->field_2C->field_8->field_F0;
    if (work->field_6E2 == 0) {
        work->field_6E2 = -1;
    }
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, work->field_6E2);
}
