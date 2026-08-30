#ifndef ACTOR_403600_H
#define ACTOR_403600_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor403600Obj2C {
    /* 0x00 */ byte pad_0[0xC];
    /* 0x0C */ s16  field_C;
} Actor403600Obj2C;

typedef struct Actor403600Work {
    /* 0x000 */ byte          pad_0[0x4B8];
    /* 0x4B8 */ GsCOORDINATE2 field_4B8;
    /* 0x508 */ byte          pad_508[0x228];
    /* 0x730 */ s16           field_730;
    /* 0x732 */ byte          pad_732[0x48];
    /* 0x77A */ s16           field_77A;
    /* 0x77C */ byte          pad_77C[0x30];
    /* 0x7AC */ s16           field_7AC;
} Actor403600Work;

typedef struct Actor403600 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor403600Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor403600Obj2C* field_2C;
} Actor403600;

typedef struct Actor403600Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor403600Ctx;

void func_actor_403600_8013938C(Actor403600Ctx* arg0, Actor403600* arg1);

#endif
