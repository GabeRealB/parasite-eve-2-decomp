#ifndef ACTORS_SHARED_801342A4_H
#define ACTORS_SHARED_801342A4_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared801342a4Obj {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} ActorShared801342a4Obj;

typedef struct ActorShared801342a4Work {
    /* 0x000 */ byte pad_0[0x35A];
    /* 0x35A */ s16  field_35A;
    /* 0x35C */ s16  field_35C;
    /* 0x35E */ byte pad_35E[0x18];
    /* 0x376 */ s16  field_376;
    /* 0x378 */ s16  field_378;
    /* 0x37A */ byte pad_37A[4];
    /* 0x37E */ s16  field_37E;
} ActorShared801342a4Work;

typedef struct ActorShared801342a4 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared801342a4Work* field_1C;
    /* 0x20 */ byte                     pad_20[0xC];
    /* 0x2C */ ActorShared801342a4Obj*  field_2C;
    /* 0x30 */ s32                      field_30;
} ActorShared801342a4;

typedef struct ActorShared801342a4Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} ActorShared801342a4Ctx;

void ActorsShared801342a4(ActorShared801342a4Ctx* arg0, ActorShared801342a4* arg1);

#endif
