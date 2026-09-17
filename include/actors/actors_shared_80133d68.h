#ifndef ACTORS_SHARED_80133D68_H
#define ACTORS_SHARED_80133D68_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared80133d68Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80133d68Obj2C;

/// The work block's target position (`field_23C`/`field_240`), the heading
/// towards it (`field_244`), the current heading (`field_246`) and the turn
/// rate (`field_254`); the same offsets as `Actor103700Work`.
typedef struct ActorShared80133d68Work {
    /* 0x000 */ byte pad_0[0x23C];
    /* 0x23C */ s32  field_23C;
    /* 0x240 */ s32  field_240;
    /* 0x244 */ u16  field_244;
    /* 0x246 */ s16  field_246;
    /* 0x248 */ byte pad_248[0xC];
    /* 0x254 */ s16  field_254;
} ActorShared80133d68Work;

typedef struct ActorShared80133d68 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80133d68Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80133d68Obj2C* field_2C;
} ActorShared80133d68;

void ActorsShared80133d68(ActorShared80133d68* arg0);

#endif
