#ifndef ACTOR_510900_H
#define ACTOR_510900_H

#include "common.h"

typedef struct Actor510900Coord {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x4C];
    /* 0x50 */ s32  field_50;
} Actor510900Coord;

typedef struct Actor510900Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor510900Coord* field_8;
    /* 0x0C */ s16               field_C;
} Actor510900Obj2C;

typedef struct Actor510900Work {
    /* 0x000 */ byte pad_0[0x584];
    /* 0x584 */ s16  field_584;
    /* 0x586 */ byte pad_586[8];
    /* 0x58E */ s16  field_58E;
    /* 0x590 */ byte pad_590[2];
    /* 0x592 */ s16  field_592;
    /* 0x594 */ byte pad_594[0x10];
    /* 0x5A4 */ s16  field_5A4;
} Actor510900Work;

typedef struct Actor510900 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor510900Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor510900Obj2C* field_2C;
} Actor510900;

typedef struct Actor510900Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor510900Ctx;

void func_actor_510900_8013B6A0(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B870(Actor510900* arg0);
s16  func_actor_510900_8013BE84(Actor510900* arg0);

#endif
