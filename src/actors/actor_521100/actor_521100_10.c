#include "common.h"

#include "actors/actor_521100.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Draws the actor's ground shadow: a quad at the second model coordinate's
/// x/z and the first's y, so it lies on the ground under the actor.
void func_actor_521100_80135A90(Actor521100* arg0)
{
    GsCOORDINATE2* coord;
    GsCOORDINATE2* sub;
    VECTOR3        vec;

    coord  = &arg0->field_2C->field_8[0];
    sub    = &arg0->field_2C->field_8[1];
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, 0x80);
}
