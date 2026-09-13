#ifndef ACTOR_202600_H
#define ACTOR_202600_H

#include "common.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor202600Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} Actor202600Obj2C;

/// Per-spawn parameters; `field_F` selects the row of the overlay's
/// per-actor data table.
typedef struct Actor202600Params {
    /* 0x00 */ byte pad_0[0xF];
    /* 0x0F */ u8   field_F;
} Actor202600Params;

typedef struct Actor202600Ctx {
    /* 0x00 */ byte               pad_0[8];
    /* 0x08 */ u16                field_8;
    /* 0x0A */ byte               pad_A[0x32];
    /* 0x3C */ Actor202600Params* field_3C;
} Actor202600Ctx;

/// The work starts with a linked `GpObj` and its 0x18-byte collision record,
/// the same prefix `Actor105500Work` has: `func_actor_202600_8014C5A0` clears
/// `obj.flags` bit 0x8000 and wipes `rec` before re-linking, then counts
/// `field_38` frames of homing movement and decays `field_3A`.
typedef struct Actor202600Work {
    /* 0x000 */ GpObj     obj;
    /* 0x020 */ GpRec18   rec;
    /* 0x038 */ s16       field_38;
    /* 0x03A */ s16       field_3A;
    /* 0x03C */ byte      pad_3C[0x330];
    /* 0x36C */ TaskDesc* field_36C;
    /* 0x370 */ byte      pad_370[0x22];
    /* 0x392 */ s16       field_392;
    /* 0x394 */ s16       field_394;
    /* 0x396 */ u16       field_396;
    /* 0x398 */ s16       field_398;
    /* 0x39A */ s16       field_39A;
    /* 0x39C */ s16       field_39C;
    /* 0x39E */ s16       field_39E;
    /* 0x3A0 */ byte      pad_3A0[2];
    /* 0x3A2 */ s16       field_3A2;
    /* 0x3A4 */ s16       field_3A4;
    /* 0x3A6 */ s16       field_3A6;
    /* 0x3A8 */ byte      pad_3A8[4];
    /* 0x3AC */ u16       field_3AC;
    /* 0x3AE */ byte      pad_3AE[2];
    /* 0x3B0 */ s16       field_3B0;
    /* 0x3B2 */ byte      pad_3B2[0xE];
    /* 0x3C0 */ s16       field_3C0;
    /* 0x3C2 */ byte      pad_3C2[6];
    /* 0x3C8 */ s16       field_3C8;
    /* 0x3CA */ byte      pad_3CA[6];
    /* 0x3D0 */ s16       field_3D0;
} Actor202600Work;

typedef struct Actor202600 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor202600Work*  field_1C;
    /* 0x20 */ Actor202600Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor202600Obj2C* field_2C;
    /* 0x30 */ s32               field_30;
} Actor202600;

extern u32 Gp_LcgState;
typedef struct Actor202600RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor202600RotScratch;
STATIC_ASSERT_SIZEOF(Actor202600RotScratch, 0x18);

extern u16 D_actor_202600_80152798[];
extern u16 D_actor_202600_801527A8[];
extern s16 D_actor_202600_80152836;
extern u8  D_801153F2[2];

#endif
