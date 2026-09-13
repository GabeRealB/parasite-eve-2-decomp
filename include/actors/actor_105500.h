#ifndef ACTOR_105500_H
#define ACTOR_105500_H

#include "common.h"
#include "gameplay/1BC.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor105500Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor105500Obj2C;

typedef struct Actor105500Work {
    /* 0x000 */ byte    pad_0[0x35C];
    /* 0x35C */ VECTOR3 field_35C;
    /* 0x368 */ byte    pad_368[0x2A];
    /* 0x392 */ s16     field_392;
    /* 0x394 */ byte    pad_394[4];
    /* 0x398 */ s16     field_398;
    /* 0x39A */ s16     field_39A;
    /* 0x39C */ s16     field_39C;
    /* 0x39E */ s16     field_39E;
    /* 0x3A0 */ byte    pad_3A0[6];
    /* 0x3A6 */ s16     field_3A6;
    /* 0x3A8 */ s16     field_3A8;
    /* 0x3AA */ byte    pad_3AA[6];
    /* 0x3B0 */ s16     field_3B0;
    /* 0x3B2 */ byte    pad_3B2[0x20];
    /* 0x3D2 */ s16     field_3D2;
} Actor105500Work;

typedef struct Actor105500 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor105500Work*  field_1C;
    /* 0x20 */ void*             field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor105500Obj2C* field_2C;
    /* 0x30 */ s32               field_30;
} Actor105500;

typedef struct Actor105500StateFuncTable3 {
    void (*funcs[3])(void*, Actor105500*);
} Actor105500StateFuncTable3;

extern Actor105500StateFuncTable3 Actor05500_D0002C;
extern Actor105500StateFuncTable3 Actor05500_D00038;

typedef struct Actor105500Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor105500Ctx;

void Actor05500_Fn03560(Actor105500Ctx* arg0, Actor105500* arg1);

extern GpU16Pair Actor05500_D08958;
void             Actor05500_Fn03E34(GpEnemy* enemy, Task* task);

#endif
