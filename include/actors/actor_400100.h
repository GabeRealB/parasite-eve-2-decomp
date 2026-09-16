#ifndef ACTOR_400100_H
#define ACTOR_400100_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "gameplay/3FB8.h"
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
    /* 0x38 */ GpRec18 field_38;
    /* 0x50 */ byte    pad_50[0x48];
} Actor00100Obj;
STATIC_ASSERT_SIZEOF(Actor00100Obj, 0x98);

typedef struct Actor00100AnimCommand {
    /* 0x00 */ byte pad_0[0x10];
    /* 0x10 */ s32  field_10;
    /* 0x14 */ s32  field_14;
} Actor00100AnimCommand;

typedef struct Actor00100Work {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /* 0x006 */ s16 field_6;
    /* 0x008 */ union {
        s16  field_8;
        byte pad_8[0x52];
    } state;
    /* 0x05A */ u16                    field_5A;
    /* 0x05C */ byte                   pad_5C[0xC];
    /* 0x068 */ u16                    field_68;
    /* 0x06A */ byte                   pad_6A[0x402];
    /* 0x46C */ u16                    field_46C;
    /* 0x46E */ byte                   pad_46E[0x3BA];
    /* 0x828 */ u16                    field_828;
    /* 0x82A */ u16                    field_82A;
    /* 0x82C */ s16                    field_82C;
    /* 0x82E */ u16                    field_82E;
    /* 0x830 */ u16                    field_830;
    /* 0x832 */ u16                    field_832;
    /* 0x834 */ u16                    field_834;
    /* 0x836 */ s16                    field_836;
    /* 0x838 */ s16                    field_838;
    /* 0x83A */ u16                    field_83A;
    /* 0x83C */ byte                   pad_83C[2];
    /* 0x83E */ u16                    field_83E;
    /* 0x840 */ u16                    field_840;
    /* 0x842 */ s16                    field_842;
    /* 0x844 */ u16                    field_844;
    /* 0x846 */ byte                   pad_846[6];
    /* 0x84C */ u32                    field_84C;
    /* 0x850 */ byte                   pad_850[0x48];
    /* 0x898 */ SVECTOR                field_898;
    /* 0x8A0 */ byte                   pad_8A0[8];
    /* 0x8A8 */ SVECTOR                field_8A8;
    /* 0x8B0 */ SVECTOR                field_8B0;
    /* 0x8B8 */ byte                   pad_8B8[0x20];
    /* 0x8D8 */ s32                    field_8D8;
    /* 0x8DC */ s32                    field_8DC;
    /* 0x8E0 */ s32                    field_8E0;
    /* 0x8E4 */ byte                   pad_8E4[4];
    /* 0x8E8 */ s16                    field_8E8;
    /* 0x8EA */ byte                   field_8EA;
    /* 0x8EB */ byte                   pad_8EB[0x1D];
    /* 0x908 */ Actor00100Obj          objs[4];
    /* 0xB68 */ byte                   pad_B68[0x38];
    /* 0xBA0 */ MATRIX                 field_BA0;
    /* 0xBC0 */ byte                   pad_BC0[0x20];
    /* 0xBE0 */ s16                    field_BE0;
    /* 0xBE2 */ byte                   pad_BE2[2];
    /* 0xBE4 */ u16                    field_BE4;
    /* 0xBE6 */ byte                   pad_BE6[0x12];
    /* 0xBF8 */ Actor00100AnimCommand* field_BF8;
    /* 0xBFC */ s32                    field_BFC;
    /* 0xC00 */ s32                    field_C00;
    /* 0xC04 */ s32                    field_C04;
    /* 0xC08 */ byte                   pad_C08[4];
    /// Last message opcode/operands, kept for the debug display: the three
    /// bytes of `Actor00100Msg` are latched here verbatim.
    /* 0xC0C */ u8   field_C0C;
    /* 0xC0D */ u8   field_C0D;
    /* 0xC0E */ u8   field_C0E;
    /* 0xC0F */ byte pad_C0F[9];
    /* 0xC18 */ s16  field_C18;
    /* 0xC1A */ s16  field_C1A;
    /* 0xC1C */ byte pad_C1C[2];
    /* 0xC1E */ u16  field_C1E;
    /* 0xC20 */ u16  field_C20;
    /* 0xC22 */ s16  field_C22;
    /* 0xC24 */ u16  field_C24;
    /* 0xC26 */ s16  field_C26;
    /* 0xC28 */ s16  field_C28;
    /* 0xC2A */ s16  field_C2A;
} Actor00100Work;

typedef struct Actor00100Record {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x14];
} Actor00100Record;
STATIC_ASSERT_SIZEOF(Actor00100Record, 0x18);

/// Alternate view of the work block: this table overlaps the object storage.
typedef struct Actor00100RecordWork {
    /* 0x000 */ byte             pad_0[0xB0C];
    /* 0xB0C */ Actor00100Record records[5];
} Actor00100RecordWork;

typedef struct Actor00100Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
    /* 0x0A */ byte pad_A[6];
    /* 0x10 */ byte field_10[4];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x2B];
    /* 0x40 */ u16  field_40;
    /* 0x42 */ byte pad_42[0xA];
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

typedef struct Actor00100AngleScratch {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 z;
    /* 0x06 */ s16 pad_6;
    /* 0x08 */ s16 yaw;
    /* 0x0A */ s16 targetYaw;
    /* 0x0C */ s16 delta;
    /* 0x0E */ s16 pad_E;
} Actor00100AngleScratch;
STATIC_ASSERT_SIZEOF(Actor00100AngleScratch, 0x10);

typedef struct Actor00100TurnScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s16     delta;
    /* 0x0A */ s16     yaw;
    /* 0x0C */ s16     steps;
    /* 0x0E */ s16     pad;
} Actor00100TurnScratch;
STATIC_ASSERT_SIZEOF(Actor00100TurnScratch, 0x10);

typedef struct Actor00100ProjectScratch {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 z;
    /* 0x06 */ s16 pad_6;
    /* 0x08 */ s16 screenX;
    /* 0x0A */ s16 screenY;
    /* 0x0C */ s32 dp;
    /* 0x10 */ s32 flag;
    /* 0x14 */ s32 pad_14;
    /* 0x18 */ s32 depth;
    /* 0x1C */ s16 yaw;
    /* 0x1E */ s16 targetYaw;
    /* 0x20 */ s16 delta;
    /* 0x22 */ s16 pad_22;
} Actor00100ProjectScratch;
STATIC_ASSERT_SIZEOF(Actor00100ProjectScratch, 0x24);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by `Actor00100_Fn00A54`:
/// the `GpDeltaScratch` filled by `func_800E0C10` plus the returned flag,
/// set when the X or Z delta is nonzero.
typedef struct Actor00100DeltaFlag {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor00100DeltaFlag;
STATIC_ASSERT_SIZEOF(Actor00100DeltaFlag, 0x14);

/// 0x54-byte scratch from `G_SCRATCH_HEAD` used by `Actor00100_Fn00508` to push
/// a coordinate away from the obstacles in a `GpRec18` table. `angle`/`ok` hold
/// up to eight bearings collected from the records, `i`/`j` are the loop
/// cursors, and `blocked` is set when any record's kind is 0x10000.
typedef struct Actor00100AvoidScratch {
    /* 0x00 */ MATRIX   m;
    /* 0x20 */ SVECTOR  dir;
    /* 0x28 */ SVECTOR3 eye;
    /* 0x2E */ byte     pad_2E[0x2];
    /* 0x30 */ s32      kind;
    /* 0x34 */ s16      angle[8];
    /* 0x44 */ s8       ok[8];
    /* 0x4C */ s16      face;
    /* 0x4E */ s16      diff;
    /* 0x50 */ u8       i;
    /* 0x51 */ u8       j;
    /* 0x52 */ u8       count;
    /* 0x53 */ u8       blocked;
} Actor00100AvoidScratch;
STATIC_ASSERT_SIZEOF(Actor00100AvoidScratch, 0x54);

/// 0x10-byte scratch the bearing helpers of `Actor00100_Fn00508` nest inside
/// `Actor00100AvoidScratch`: an obstacle's offset, widened to words.
typedef struct Actor00100AvoidDelta {
    /* 0x0 */ s32  vx;
    /* 0x4 */ s32  vy;
    /* 0x8 */ s32  vz;
    /* 0xC */ byte pad_C[0x4];
} Actor00100AvoidDelta;
STATIC_ASSERT_SIZEOF(Actor00100AvoidDelta, 0x10);

/// One halfword of an `Actor00100Msg`, which the message system also hands to
/// handlers as a raw byte triple.
typedef union Actor00100MsgWord {
    /* 0x00 */ u16 word;
    /* 0x00 */ u8  bytes[2];
} Actor00100MsgWord;

/// Payload of the messages `Actor00100_Fn00E58` dispatches on. `field_0` is the
/// opcode (0x104 / 0x109 / 0x202 / 0x1602) and `field_2` the sub-command.
typedef struct Actor00100Msg {
    /* 0x00 */ Actor00100MsgWord field_0;
    /* 0x02 */ Actor00100MsgWord field_2;
} Actor00100Msg;
STATIC_ASSERT_SIZEOF(Actor00100Msg, 0x4);

/// One entry of `Actor00100_D00004`: a translation plus the yaw applied after
/// it. The first component is signed, the rest are not (the code sign-extends
/// them at the use site).
typedef struct Actor00100PoseRow {
    /* 0x00 */ s16 vx;
    /* 0x02 */ u16 vy;
    /* 0x04 */ u16 vz;
    /* 0x06 */ u16 yaw;
} Actor00100PoseRow;
STATIC_ASSERT_SIZEOF(Actor00100PoseRow, 0x8);

/// Four poses `Actor00100_Fn00E58` picks between when the 0x104 message arms
/// the actor. Alignment stays 2 so a whole-table copy stays unaligned.
typedef struct Actor00100PoseTable {
    /* 0x00 */ Actor00100PoseRow rows[4];
} Actor00100PoseTable;
STATIC_ASSERT_SIZEOF(Actor00100PoseTable, 0x20);

/// Source of the four halfwords the 0x1602 handler latches into
/// `Actor00100Work.field_C1E..field_C24`.
typedef struct Actor00100PoseSrc {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  field_18;
    /* 0x1A */ u16  field_1A;
    /* 0x1C */ u16  field_1C;
    /* 0x1E */ u16  field_1E;
} Actor00100PoseSrc;

extern Actor00100PoseTable Actor00100_D00004;
extern Actor00100PoseSrc   Actor00100_D0BDB4;

extern char  Actor00100_D10D60;
extern char  Actor00100_D11234;
extern char  Actor00100_D11F90;
extern char  Actor00100_D12470;
extern void* D_80114B78[1];

void Actor00100_Fn09310(Actor00100* arg0);

extern u32 Gp_LcgState;

void Actor00100_Fn04270(Actor00100* argx);
s32  Actor00100_Fn00A54(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2);
void Actor00100_Fn02788(Actor00100* arg0);
void Actor00100_Fn0B658(Actor00100* arg0);
s32  Actor00100_Fn0B264(Task* task);
s32  Actor00100_Fn0B1A4(Actor00100* arg0, s32 arg1, s32 arg2);

s32 Actor00100_Fn00508(GsCOORDINATE2* coord, GpRec18* records, s16 count, SVECTOR* pos);

#endif
