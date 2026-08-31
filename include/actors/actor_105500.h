#ifndef ACTOR_105500_H
#define ACTOR_105500_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor105500Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor105500Obj2C;

typedef struct Actor105500Work {
    /* 0x000 */ byte pad_0[0x39A];
    /* 0x39A */ s16  field_39A;
    /* 0x39C */ byte pad_39C[0xA];
    /* 0x3A6 */ s16  field_3A6;
    /* 0x3A8 */ byte pad_3A8[8];
    /* 0x3B0 */ s16  field_3B0;
} Actor105500Work;

typedef struct Actor105500 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor105500Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor105500Obj2C* field_2C;
} Actor105500;

typedef struct Actor105500Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor105500Ctx;

void Actor05500_Fn03560(Actor105500Ctx* arg0, Actor105500* arg1);

#endif
