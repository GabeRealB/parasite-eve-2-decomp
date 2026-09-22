#include "common.h"

#include "actors/actors_shared_80134eb8.h"

void ActorsShared80134eb8(ActorShared80134eb8* arg0)
{
    ActorShared80134eb8Work* work;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           c2;
    GsCOORDINATE2*           c3;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    c2    = coord + 2;
    c3    = coord + 3;

    c2->coord.t[0] = 0;
    c2->coord.t[1] = -0x5F;
    if (work->field_134 != 0) {
        c2->coord.t[2] += 0x14;
    } else {
        c2->coord.t[2] -= 0x28;
        if (c2->coord.t[2] < 0) {
            c2->coord.t[2] = 0;
        }
    }
    c2->flg        = 0;
    c3->coord.t[0] = 0;
    c3->coord.t[1] = 0;
    if (work->field_134 != 0) {
        c3->coord.t[2] += 0x50;
    } else {
        c3->coord.t[2] -= 0xA0;
        if (c3->coord.t[2] < 0x1E) {
            c3->coord.t[2] = 0x1E;
        }
    }
    c3->flg = 0;
}
