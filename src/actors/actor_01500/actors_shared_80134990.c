#include "common.h"

#include "actors/actor_101500.h"
#include "gameplay/3CD8.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Ground shadow for the actor: carves a `VECTOR3` off the scratchpad and fills
/// it from the root coordinate's world translation - straight out of `workm.t`
/// in state 5, otherwise from the hit point `func_800EA1A8` finds casting a
/// ray down. The shade passed to `Gp_DrawEffGroundQuad` is `0x80` in state 5,
/// otherwise `func_800EA318`'s reading of the ray's drop.
void Actor01500_Fn02B70(Actor101500* arg0)
{
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    VECTOR3*         vec;
    void*            head;
    s16              hit;

    head                = *(void**)0x1F8003FC;
    work                = arg0->field_1C;
    coord               = arg0->field_2C->coords;
    *(void**)0x1F8003FC = (u8*)head - 0x10;
    vec                 = (VECTOR3*)((u8*)head - 0x10);
    if (work->field_35A != 5) {
        hit = func_800EA1A8((VECTOR3*)coord->workm.t, vec);
        if (hit != 0) {
            Gp_DrawEffGroundQuad(vec, 0x200, func_800EA318(0x200, 0x80, hit));
        }
    } else {
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0x80);
    }
    *(void**)0x1F8003FC += 0x10;
}
