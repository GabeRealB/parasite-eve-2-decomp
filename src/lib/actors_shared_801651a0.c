#include "common.h"

#include "actors/actor_100700.h"
#include "actors/actor_100700_spawn.h"
#include "main/tmd.h"
#include "actors/actors_shared_80135b58.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "main/display.h"
#include "psyq/inline_c.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

void ActorsShared801651a0(Actor00700* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;

    coord              = arg0->field_2C->field_8;
    work               = arg0->field_1C;
    work->field_360    = coord->coord.t[0];
    work->field_364    = coord->coord.t[1];
    work->field_368    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_384) >> 0xC;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_384) >> 0xC;
}
