#ifndef ACTOR_400100_H
#define ACTOR_400100_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "main/tmd.h"

/// 0x98-byte object embedded four times in `Actor00100Work` at 0x908. The
/// leading 0x20 bytes are the `GpObj` list node unlinked by `Gp_UnlinkObj`;
/// `flags` is that node's flag halfword.
typedef struct Actor00100Obj {
    /* 0x00 */ byte pad_0[0x1E];
    /* 0x1E */ u16  flags;
    /* 0x20 */ byte pad_20[0x78];
} Actor00100Obj;
STATIC_ASSERT_SIZEOF(Actor00100Obj, 0x98);

typedef struct Actor00100Work {
    /* 0x000 */ s16           field_0;
    /* 0x002 */ byte          pad_2[2];
    /* 0x004 */ s16           field_4;
    /* 0x006 */ s16           field_6;
    /* 0x008 */ byte          pad_8[0x890];
    /* 0x898 */ SVECTOR       field_898;
    /* 0x8A0 */ byte          pad_8A0[8];
    /* 0x8A8 */ SVECTOR       field_8A8;
    /* 0x8B0 */ SVECTOR       field_8B0;
    /* 0x8B8 */ byte          pad_8B8[0x50];
    /* 0x908 */ Actor00100Obj objs[4];
    /* 0xB68 */ byte          pad_B68[0xB0];
    /* 0xC18 */ s16           field_C18;
} Actor00100Work;

typedef struct Actor00100Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x3F];
    /* 0x54 */ s32  field_54;
} Actor00100Ctx;

typedef struct Actor00100 {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Actor00100Work* field_1C;
    /* 0x20 */ Actor00100Ctx*  field_20;
    /* 0x24 */ byte            pad_24[8];
    /* 0x2C */ TmdObject*      field_2C;
    /* 0x30 */ s32             field_30;
} Actor00100;

/// 0x34-byte block taken from `G_SCRATCH_HEAD`: a `MATRIX` plus the `VECTOR`
/// handed to `ScaleMatrix` and the yaw stored before `Gfx_RotMatrixY`.
typedef struct Actor00100MtxScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor00100MtxScratch;
STATIC_ASSERT_SIZEOF(Actor00100MtxScratch, 0x34);

void Actor00100_Fn04270(Actor00100* argx);
s32  Actor00100_Fn0B1A4(Actor00100* arg0, s32 arg1, s32 arg2);

#endif
