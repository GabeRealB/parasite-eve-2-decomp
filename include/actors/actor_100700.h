#ifndef ACTOR_100700_H
#define ACTOR_100700_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor00700Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor00700Obj2C;

typedef struct Actor00700Work {
    /* 0x000 */ byte pad_0[0x386];
    /* 0x386 */ s16  field_386;
} Actor00700Work;

typedef struct Actor00700 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor00700Work*  field_1C;
    /* 0x20 */ byte             pad_20[0xC];
    /* 0x2C */ Actor00700Obj2C* field_2C;
} Actor00700;

typedef struct Actor00700Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor00700Ctx;

void Actor00700_Fn0188C(Actor00700Ctx* arg0, Actor00700* arg1);

#endif
