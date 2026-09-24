#include "common.h"

#include "actors/actor_510900.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Draws the actor's ground shadow: a 0x300-wide quad at shade 0x80, placed at
/// the second coordinate's x/z and the first coordinate's y, so it lies on the
/// ground under the body even when the two coordinates are apart.
void func_actor_510900_8013BBE4(Actor510900* arg0)
{
    GsCOORDINATE2* coord;
    GsCOORDINATE2* sub;
    VECTOR3        vec;

    coord  = &arg0->field_2C->field_8->field_0;
    sub    = &arg0->field_2C->field_8->field_50;
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, 0x80);
}
