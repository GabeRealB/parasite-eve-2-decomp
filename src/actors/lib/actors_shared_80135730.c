#include "common.h"

#include "actors/actors_shared_80135730.h"

/// Steps the actor's coordinate forward, saving the previous position first.
/// The horizontal step follows the coordinate's own forward axis
/// (`coord.m[*][2]`) scaled by ActorShared80135730Work::field_398; the vertical
/// step is a plain per-frame delta.
void ActorsShared80135730(ActorShared80135730* arg0)
{
    GsCOORDINATE2*           coord;
    ActorShared80135730Work* work;

    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;

    work->field_35C.vx = coord->coord.t[0];
    work->field_35C.vy = coord->coord.t[1];
    work->field_35C.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_398) >> 12;
    coord->coord.t[1] += work->field_3A8;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_398) >> 12;
}
