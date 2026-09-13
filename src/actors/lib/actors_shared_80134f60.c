#include "common.h"

#include "actors/actors_shared_80134f60.h"

void ActorsShared80134f60(ActorShared80134f60* arg0)
{
    ActorShared80134f60Work* work;
    GsCOORDINATE2*           coord;

    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;

    work->field_120    = coord->coord.t[0];
    work->field_122    = coord->coord.t[1];
    work->field_124    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_138) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_138) >> 12;
}
