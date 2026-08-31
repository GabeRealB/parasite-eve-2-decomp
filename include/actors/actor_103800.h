#ifndef ACTOR_103800_H
#define ACTOR_103800_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor103800Obj2C {
    /* 0x00 */ byte pad_0[0xC];
    /* 0x0C */ s16  field_C;
} Actor103800Obj2C;

typedef struct Actor103800Work {
    /* 0x000 */ byte           pad_0[0x344];
    /* 0x344 */ GsCOORDINATE2* field_344;
    /* 0x348 */ byte           pad_348[0x18];
    /* 0x360 */ s16            field_360;
    /* 0x362 */ byte           pad_362[8];
    /* 0x36A */ s16            field_36A;
} Actor103800Work;

typedef struct Actor103800 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor103800Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor103800Obj2C* field_2C;
} Actor103800;

typedef struct Actor103800Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor103800Ctx;

void Actor03800_Fn031B8(Actor103800Ctx* arg0, Actor103800* arg1);

#endif
