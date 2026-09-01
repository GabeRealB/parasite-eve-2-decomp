#ifndef ACTOR_521100_H
#define ACTOR_521100_H

#include "common.h"

typedef struct Actor521100Coord {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x4C];
    /* 0x50 */ s32  field_50;
} Actor521100Coord;

typedef struct Actor521100Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor521100Coord* field_8;
    /* 0x0C */ s16               field_C;
} Actor521100Obj2C;

typedef struct Actor521100 {
    /* 0x00 */ byte                    pad_0[0x1C];
    /* 0x1C */ struct Actor521100Work* field_1C;
    /* 0x20 */ byte                    pad_20[0xC];
    /* 0x2C */ Actor521100Obj2C*       field_2C;
} Actor521100;

typedef struct Actor521100Work {
    /* 0x000 */ byte         pad_0[0x654];
    /* 0x654 */ Actor521100* field_654;
    /* 0x658 */ byte         pad_658[0x28];
    /* 0x680 */ s16          field_680;
    /* 0x682 */ byte         pad_682[0x1C];
    /* 0x69E */ s16          field_69E;
    /* 0x6A0 */ byte         pad_6A0[0x10];
    /* 0x6B0 */ s16          field_6B0;
    /* 0x6B2 */ s16          field_6B2;
} Actor521100Work;

typedef struct Actor521100Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor521100Ctx;

void func_actor_521100_80135478(Actor521100Ctx* arg0, Actor521100* arg1);
void func_actor_521100_801355C8(Actor521100* arg0);

#endif
