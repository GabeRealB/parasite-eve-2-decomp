#ifndef ACTOR_400100_H
#define ACTOR_400100_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "main/tmd.h"
#include "main/task.h"
#include "main/session.h"

/// 0x98-byte object embedded four times in `Actor00100Work` at 0x908. The
/// leading 0x20 bytes are the `GpObj` list node unlinked by `Gp_UnlinkObj`;
/// `flags` is that node's flag halfword.
typedef struct Actor00100Obj {
    /* 0x00 */ byte    pad_0[0x1C];
    /* 0x1C */ u16     field_1C;
    /* 0x1E */ u16     flags;
    /* 0x20 */ GpRec18 field_20;
    /* 0x38 */ byte    pad_38[0x60];
} Actor00100Obj;
STATIC_ASSERT_SIZEOF(Actor00100Obj, 0x98);

typedef struct Actor00100Work {
    /* 0x000 */ s16           field_0;
    /* 0x002 */ byte          pad_2[2];
    /* 0x004 */ s16           field_4;
    /* 0x006 */ s16           field_6;
    /* 0x008 */ byte          pad_8[0x52];
    /* 0x05A */ u16           field_5A;
    /* 0x05C */ byte          pad_5C[0xC];
    /* 0x068 */ u16           field_68;
    /* 0x06A */ byte          pad_6A[0x7BE];
    /* 0x828 */ u16           field_828;
    /* 0x82A */ u16           field_82A;
    /* 0x82C */ byte          pad_82C[2];
    /* 0x82E */ u16           field_82E;
    /* 0x830 */ byte          pad_830[2];
    /* 0x832 */ u16           field_832;
    /* 0x834 */ u16           field_834;
    /* 0x836 */ byte          pad_836[0x62];
    /* 0x898 */ SVECTOR       field_898;
    /* 0x8A0 */ byte          pad_8A0[8];
    /* 0x8A8 */ SVECTOR       field_8A8;
    /* 0x8B0 */ SVECTOR       field_8B0;
    /* 0x8B8 */ byte          pad_8B8[0x50];
    /* 0x908 */ Actor00100Obj objs[4];
    /* 0xB68 */ byte          pad_B68[0xB0];
    /* 0xC18 */ s16           field_C18;
    /* 0xC1A */ byte          pad_C1A[0x10];
    /* 0xC2A */ s16           field_C2A;
} Actor00100Work;

typedef struct Actor00100Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[7];
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
void Actor00100_Fn00A54(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2);
void Actor00100_Fn02788(Actor00100* arg0);
void Actor00100_Fn0B658(Actor00100* arg0);
s32  Actor00100_Fn0B264(Task* task);
s32  Actor00100_Fn0B1A4(Actor00100* arg0, s32 arg1, s32 arg2);

#endif
