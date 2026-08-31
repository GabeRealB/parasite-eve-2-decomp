#ifndef ACTOR_102500_H
#define ACTOR_102500_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor02500Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor02500Obj2C;

typedef struct Actor02500Work {
    /* 0x000 */ byte pad_0[0x322];
    /* 0x322 */ s16  field_322;
    /* 0x324 */ byte pad_324[4];
    /* 0x328 */ s16  field_328;
} Actor02500Work;

typedef struct Actor02500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor02500Work*  field_1C;
    /* 0x20 */ byte             pad_20[0xC];
    /* 0x2C */ Actor02500Obj2C* field_2C;
    /* 0x30 */ s32              field_30;
} Actor02500;

typedef struct Actor02500Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor02500Ctx;

void Actor02500_Fn01E60(Actor02500Ctx* arg0, Actor02500* arg1);

#endif
