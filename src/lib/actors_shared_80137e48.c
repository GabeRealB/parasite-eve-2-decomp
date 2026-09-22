#include "common.h"

#include "actors/actors_shared_80137e48.h"

void ActorsShared80137e48(ActorShared80137e48* arg0)
{
    ActorShared80137e48Work* work;
    GsCOORDINATE2*           coord;

    coord              = arg0->field_2C->field_8;
    work               = arg0->field_1C;
    work->field_664    = coord->coord.t[0];
    work->field_668    = coord->coord.t[1];
    work->field_66C    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_6C8) >> 12;
    if (work->field_714 < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_6C8) >> 12;
}
