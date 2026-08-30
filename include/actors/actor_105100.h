#ifndef ACTOR_105100_H
#define ACTOR_105100_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor105100Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor105100Obj2C;

typedef struct Actor105100Work {
    /* 0x000 */ byte pad_0[0x560];
    /* 0x560 */ s32  field_560;
    /* 0x564 */ byte pad_564[0x30];
    /* 0x594 */ s16  field_594;
    /* 0x596 */ byte pad_596[0xC];
    /* 0x5A2 */ s16  field_5A2;
    /* 0x5A4 */ byte pad_5A4[4];
    /* 0x5A8 */ s16  field_5A8;
    /* 0x5AA */ byte pad_5AA[0x12];
    /* 0x5BC */ s16  field_5BC;
} Actor105100Work;

typedef struct Actor105100 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor105100Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor105100Obj2C* field_2C;
} Actor105100;

typedef struct Actor105100Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor105100Ctx;

void func_actor_105100_80132AA0(Actor105100Ctx* arg0, Actor105100* arg1);

#endif
