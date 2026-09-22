#include "common.h"

#include "actors/actors_shared_80138640.h"

/// Steps the actor's coordinate forward, saving the previous position first.
/// The horizontal step follows the coordinate's own forward axis
/// (`coord.m[*][2]`) scaled by `ActorShared80138640Work::field_378`; the
/// vertical step is a plain per-frame delta.
void ActorsShared80138640(ActorShared80138640* arg0)
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
