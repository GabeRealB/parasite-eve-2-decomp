#include "common.h"

#include "actors/actors_shared_80138640.h"

/// Steps the second form's root one frame: saves the current translation in
/// `field_33C`, advances X and Z along the rotation's Z column scaled by the
/// step length `field_378`, and Y by the fall speed `field_398`.
void Actor07000_Fn06820(ActorShared80138640* arg0)
{
    GsCOORDINATE2*           coord;
    ActorShared80138640Work* work;

    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;

    work->field_33C.vx = coord->coord.t[0];
    work->field_33C.vy = coord->coord.t[1];
    work->field_33C.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_378) >> 12;
    coord->coord.t[1] += work->field_398;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_378) >> 12;
}
