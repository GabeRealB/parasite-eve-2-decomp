#ifndef ACTOR_102100_H
#define ACTOR_102100_H

#include "common.h"

typedef struct Actor02100Obj2C {
    /* 0x0 */ byte pad_0[0xC];
    /* 0xC */ s16  field_C;
} Actor02100Obj2C;

typedef struct Actor02100Work {
    /* 0x000 */ byte pad_0[0x40];
    /* 0x040 */ byte field_40[0x38];
    /* 0x078 */ byte field_78[0x50];
    /* 0x0C8 */ byte field_C8[0xA0];
    /* 0x168 */ s32  field_168;
    /* 0x16C */ byte pad_16C[8];
    /* 0x174 */ s16  field_174;
    /* 0x176 */ byte pad_176[4];
    /* 0x17A */ s16  field_17A;
    /* 0x17C */ byte pad_17C[0xC];
    /* 0x188 */ s16  field_188;
    /* 0x18A */ byte pad_18A[0x12];
} Actor02100Work;
STATIC_ASSERT_SIZEOF(Actor02100Work, 0x19C);

typedef struct Actor02100 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor02100Work*  field_1C;
    /* 0x20 */ byte             pad_20[0xC];
    /* 0x2C */ Actor02100Obj2C* field_2C;
} Actor02100;
STATIC_ASSERT_SIZEOF(Actor02100, 0x30);

typedef struct Actor02100Ctx {
    /* 0x00 */ byte pad_0[0x10];
    /* 0x10 */ byte node[8];
    /* 0x18 */ byte pad_18[0x3C];
    /* 0x54 */ s32  field_54;
} Actor02100Ctx;
STATIC_ASSERT_SIZEOF(Actor02100Ctx, 0x58);

void Actor02100_Fn035D4(Actor02100Ctx* arg0, Actor02100* arg1);

#endif
