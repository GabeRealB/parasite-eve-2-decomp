#ifndef ACTOR_300700_H
#define ACTOR_300700_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor300700Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor300700Obj2C;

typedef struct Actor300700Work {
    /* 0x000 */ byte   pad_0[0x22C];
    /* 0x22C */ MATRIX field_22C;
    /* 0x24C */ byte   pad_24C[0x96];
    /* 0x2E2 */ s16    field_2E2;
    /* 0x2E4 */ byte   pad_2E4[0x96];
    /* 0x37A */ s16    field_37A;
    /* 0x37C */ s16    field_37C;
    /* 0x37E */ u16    field_37E;
    /* 0x380 */ s16    field_380;
    /* 0x382 */ u16    field_382;
    /* 0x384 */ s16    field_384;
    /* 0x386 */ s16    field_386;
    /* 0x388 */ byte   pad_388[4];
    /* 0x38C */ u16    field_38C;
    /* 0x38E */ byte   pad_38E[4];
    /* 0x392 */ u16    field_392;
    /* 0x394 */ s16    field_394;
} Actor300700Work;

typedef struct Actor300700Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor300700Ctx;

typedef struct Actor300700 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor300700Work*  field_1C;
    /* 0x20 */ Actor300700Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor300700Obj2C* field_2C;
} Actor300700;

void func_actor_300700_80164D3C(Actor300700Ctx* arg0, Actor300700* arg1);

#endif
