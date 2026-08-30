#ifndef ACTOR_102600_H
#define ACTOR_102600_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor102600Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor102600Obj2C;

typedef struct Actor102600Work {
    /* 0x000 */ byte pad_0[0x3A6];
    /* 0x3A6 */ s16  field_3A6;
    /* 0x3A8 */ byte pad_3A8[8];
    /* 0x3B0 */ s16  field_3B0;
} Actor102600Work;

typedef struct Actor102600 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor102600Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor102600Obj2C* field_2C;
} Actor102600;

typedef struct Actor102600Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor102600Ctx;

void func_actor_102600_80135378(Actor102600Ctx* arg0, Actor102600* arg1);

#endif
