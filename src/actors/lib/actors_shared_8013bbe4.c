#include "common.h"

#include "actors/actors_shared_8013bbe4.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Draws the actor's ground shadow. The x/z come from the second coordinate
/// and the y from the first, so the quad lies flat on the ground under the
/// actor rather than following the tilt of either part.
void ActorsShared8013bbe4(ActorShared8013bbe4* arg0)
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
