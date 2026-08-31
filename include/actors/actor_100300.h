#ifndef ACTOR_100300_H
#define ACTOR_100300_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor100300Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} Actor100300Obj2C;

typedef struct Actor100300Work {
    /* 0x000 */ byte pad_0[0x648];
    /* 0x648 */ s32  field_648;
    /* 0x64C */ byte pad_64C[0x18];
    /* 0x664 */ s16  field_664;
    /* 0x666 */ byte pad_666[0x10];
    /* 0x676 */ s16  field_676;
    /* 0x678 */ byte pad_678[0x4];
    /* 0x67C */ s16  field_67C;
    /* 0x67E */ byte pad_67E[0x6];
    /* 0x684 */ s16  field_684;
} Actor100300Work;

typedef struct Actor100300 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor100300Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor100300Obj2C* field_2C;
} Actor100300;

typedef struct Actor100300Ctx {
    /* 0x00 */ byte pad_0[0x4C];
    /* 0x4C */ u8   field_4C;
} Actor100300Ctx;

void Actor00300_Fn04958(Actor100300Ctx* arg0, Actor100300* arg1);

#endif
