#ifndef ACTOR_302600_H
#define ACTOR_302600_H

#include "common.h"
#include "gameplay/areaplace.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor302600Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} Actor302600Obj2C;

typedef struct Actor302600Ctx {
    /* 0x00 */ byte         pad_0[8];
    /* 0x08 */ u16          field_8;
    /* 0x0A */ byte         pad_A[0x32];
    /* 0x3C */ GpAreaPlace* field_3C;
} Actor302600Ctx;

typedef struct Actor302600Work {
    /* 0x000 */ byte pad_0[0x392];
    /* 0x392 */ s16  field_392;
    /* 0x394 */ byte pad_394[2];
    /* 0x396 */ u16  field_396;
    /* 0x398 */ s16  field_398;
    /* 0x39A */ s16  field_39A;
    /* 0x39C */ s16  field_39C;
    /* 0x39E */ s16  field_39E;
    /* 0x3A0 */ byte pad_3A0[2];
    /* 0x3A2 */ s16  field_3A2;
    /* 0x3A4 */ s16  field_3A4;
    /* 0x3A6 */ s16  field_3A6;
    /* 0x3A8 */ byte pad_3A8[4];
    /* 0x3AC */ u16  field_3AC;
    /* 0x3AE */ byte pad_3AE[2];
    /* 0x3B0 */ s16  field_3B0;
    /* 0x3B2 */ byte pad_3B2[0xE];
    /* 0x3C0 */ s16  field_3C0;
    /* 0x3C2 */ byte pad_3C2[6];
    /* 0x3C8 */ s16  field_3C8;
} Actor302600Work;

typedef struct Actor302600RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor302600RotScratch;
STATIC_ASSERT_SIZEOF(Actor302600RotScratch, 0x18);

typedef struct Actor302600 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor302600Work*  field_1C;
    /* 0x20 */ Actor302600Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor302600Obj2C* field_2C;
} Actor302600;

extern u32 Gp_LcgState;
extern u16 D_actor_302600_8016A798[];
extern u16 D_actor_302600_8016A7A8[];

#endif
