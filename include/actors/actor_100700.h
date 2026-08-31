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
    /* 0x000 */ byte pad_0[0x2E6];
    /* 0x2E6 */ s16  field_2E6;
    /* 0x2E8 */ byte pad_2E8[0x92];
    /* 0x37A */ s16  field_37A;
    /* 0x37C */ byte pad_37C[0xA];
    /* 0x386 */ s16  field_386;
} Actor00700Work;

typedef struct Actor00700 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor00700Work*  field_1C;
    /* 0x20 */ byte             pad_20[0xC];
    /* 0x2C */ Actor00700Obj2C* field_2C;
} Actor00700;

typedef struct Actor00700Ctx {
    /* 0x00 */ byte pad_0[0x8];
    /* 0x08 */ u16  field_8;
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor00700Ctx;

void Actor00700_Fn0188C(Actor00700Ctx* arg0, Actor00700* arg1);
void Actor00700_Fn01AB8(Actor00700* arg0);
void Actor00700_Fn02290(Actor00700Ctx* arg0, Actor00700* arg1);

#endif
