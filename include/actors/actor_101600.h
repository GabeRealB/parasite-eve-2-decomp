#ifndef ACTOR_101600_H
#define ACTOR_101600_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/tmd.h"
#include "main/task.h"
#include "main/session.h"

/// Singly-linked node embedded in `Actor01600Ctx` at 0x10 and unlinked by
/// `Gp_UnlinkNode`. Same layout as the gameplay `GpLinkNode`.
typedef struct Actor01600Node {
    /* 0x0 */ struct Actor01600Node* next;
    /* 0x4 */ u8                     field_4;
    /* 0x5 */ byte                   pad_5[3];
} Actor01600Node;
STATIC_ASSERT_SIZEOF(Actor01600Node, 0x8);

/// Fields read from this actor's spawn parameter record. Byte 1 selects the
/// variant `Actor01600_Fn05F80` runs: 1 is the main state machine, 2 and 4 run
/// their own countdown on `Actor01600Work.field_524` (byte 2 is the kind
/// `Actor01600_Fn05400` switches on).
typedef struct Actor01600Params {
    /* 0x00 */ byte pad_0[1];
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u16  field_2;
    /* 0x04 */ byte pad_4[6];
    /* 0x0A */ s16  field_A;
} Actor01600Params;

typedef struct Actor01600Desc {
    /* 0x00 */ byte pad_0[4];
    /* 0x04 */ u16  field_4;
} Actor01600Desc;

typedef struct Actor01600Ctx {
    /* 0x00 */ byte              pad_0[4];
    /* 0x04 */ MATRIX*           field_4;
    /* 0x08 */ u16               field_8;
    /* 0x0A */ byte              pad_A[0x6];
    /* 0x10 */ Actor01600Node    node;
    /* 0x18 */ GsCOORDINATE2*    field_18;
    /* 0x1C */ s32               field_1C;
    /* 0x20 */ s32               field_20;
    /* 0x24 */ s32               field_24;
    /* 0x28 */ byte              pad_28[0x14];
    /* 0x3C */ Actor01600Params* field_3C;
    /* 0x40 */ u16               field_40;
    /* 0x42 */ byte              pad_42[6];
    /* 0x48 */ u8                field_48;
    /* 0x49 */ byte              pad_49[3];
    /* 0x4C */ u8                field_4C;
    /* 0x4D */ byte              pad_4D[3];
    /* 0x50 */ Actor01600Desc*   field_50;
    /* 0x54 */ s32               field_54;
} Actor01600Ctx;
STATIC_ASSERT_SIZEOF(Actor01600Ctx, 0x58);

typedef struct Actor01600Range {
    /* 0x0 */ s32 low;
    /* 0x4 */ s32 high;
} Actor01600Range;
STATIC_ASSERT_SIZEOF(Actor01600Range, 0x8);

/// Overlay-local animation views; gameplay/1BC.h also declares incompatible
/// full gameplay actor globals. Slots begin at 0x14 of the work block.
typedef struct Actor01600AnimSlot {
    /* 0x00 */ byte pad_0[9];
    /* 0x09 */ u8   field_9;
    /* 0x0A */ byte pad_A[0x1E];
} Actor01600AnimSlot;
STATIC_ASSERT_SIZEOF(Actor01600AnimSlot, 0x28);

typedef struct Actor01600AnimCtx {
    /* 0x00 */ void*               field_0;
    /* 0x04 */ void*               field_4;
    /* 0x08 */ void*               field_8;
    /* 0x0C */ Actor01600AnimSlot* field_C;
    /* 0x10 */ void*               field_10;
} Actor01600AnimCtx;
STATIC_ASSERT_SIZEOF(Actor01600AnimCtx, 0x14);

void Gp_AnimTickIndex(Actor01600AnimCtx* arg0, s32 arg1);
void func_800B4114(Actor01600AnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Packed contact record fields over the existing 0x2EC collision block.
typedef union Actor01600ContactId {
    s32 id;
    struct {
        u8  byte0;
        u8  byte1;
        u16 kind;
    } parts;
} Actor01600ContactId;

typedef union Actor01600Contacts {
    byte field_2EC[0xE0];
    struct {
        /* 0x2EC */ byte                pad_2EC[8];
        /* 0x2F4 */ GsCOORDINATE2*      field_2F4;
        /* 0x2F8 */ s8*                 field_2F8;
        /* 0x2FC */ s16                 field_2FC;
        /* 0x2FE */ s16                 field_2FE;
        /* 0x300 */ s16                 field_300;
        /* 0x302 */ byte                pad_302[2];
        /* 0x304 */ s32                 field_304;
        /* 0x308 */ s16                 field_308;
        /* 0x30A */ u16                 field_30A;
        /* 0x30C */ byte                pad_30C[2];
        /* 0x30E */ s16                 field_30E;
        /* 0x310 */ Actor01600ContactId hit;
        /* 0x314 */ s16                 field_314;
        /* 0x316 */ s16                 field_316;
        /* 0x318 */ s16                 field_318;
        /* 0x31A */ byte                pad_31A[0xB2];
    } named;
} Actor01600Contacts;
STATIC_ASSERT_SIZEOF(Actor01600Contacts, 0xE0);

typedef union Actor01600HitVector {
    VECTOR v;
    struct {
        s16 lowx, highx, lowy, highy, lowz, highz;
        s32 pad;
    } half;
} Actor01600HitVector;

/// Collision displacement and normalized push vectors in the scratch arena.
typedef struct Actor01600HitScratch {
    /* 0x00 */ byte                pad[0x20];
    /* 0x20 */ Actor01600HitVector delta;
    /* 0x30 */ VECTOR              normal;
    /* 0x40 */ byte                tail[0xC];
} Actor01600HitScratch;
STATIC_ASSERT_SIZEOF(Actor01600HitScratch, 0x4C);

typedef struct Actor01600Work {
    /* 0x000 */ Actor01600AnimCtx  anim;
    /* 0x014 */ Actor01600AnimSlot slots[9];
    /* 0x17C */ byte               pad_17C[0x90];
    /* 0x20C */ MATRIX             field_20C;
    /* 0x22C */ MATRIX             field_22C;
    /* 0x24C */ GsCOORDINATE2      field_24C;
    /* 0x29C */ byte               field_29C[8];
    /* 0x2A4 */ GsCOORDINATE2*     field_2A4;
    /* 0x2A8 */ s8*                field_2A8;
    /* 0x2AC */ s16                field_2AC;
    /* 0x2AE */ s16                field_2AE;
    /* 0x2B0 */ s16                field_2B0;
    /* 0x2B2 */ s16                field_2B2;
    /* 0x2B4 */ s32                field_2B4;
    /* 0x2B8 */ s16                field_2B8;
    /* 0x2BA */ u16                field_2BA;
    /* 0x2BC */ byte               pad_2BC[4];
    /* 0x2C0 */ s16                field_2C0;
    /* 0x2C2 */ byte               pad_2C2[0xA];
    /* 0x2CC */ s16                field_2CC;
    /* 0x2CE */ s16                field_2CE;
    /* 0x2D0 */ GpRec18*           field_2D0;
    /* 0x2D4 */ GpRec18            field_2D4;
    /* 0x2EC */ Actor01600Contacts collision;
    /* 0x3CC */ byte               field_3CC[8];
    /* 0x3D4 */ GsCOORDINATE2*     field_3D4;
    /* 0x3D8 */ s8*                field_3D8;
    /* 0x3DC */ s16                field_3DC;
    /* 0x3DE */ s16                field_3DE;
    /* 0x3E0 */ s16                field_3E0;
    /* 0x3E2 */ s16                field_3E2;
    /* 0x3E4 */ s32                field_3E4;
    /* 0x3E8 */ s16                field_3E8;
    /* 0x3EA */ u16                field_3EA;
    /* 0x3EC */ byte               pad_3EC[0x18];
    /* 0x404 */ GsCOORDINATE2*     field_404;
    /* 0x408 */ s16                field_408;
    /* 0x40A */ s16                field_40A;
    /* 0x40C */ byte               field_40C[8];
    /* 0x414 */ GsCOORDINATE2*     field_414;
    /* 0x418 */ s8*                field_418;
    /* 0x41C */ s16                field_41C;
    /* 0x41E */ s16                field_41E;
    /* 0x420 */ s16                field_420;
    /* 0x422 */ s16                field_422;
    /* 0x424 */ s32                field_424;
    /* 0x428 */ s16                field_428;
    /* 0x42A */ u16                field_42A;
    /* 0x42C */ s16                field_42C;
    /* 0x42E */ byte               pad_42E[2];
    /* 0x430 */ s16                field_430;
    /* 0x432 */ byte               pad_432[0xA];
    /* 0x43C */ s16                field_43C;
    /* 0x43E */ s16                field_43E;
    /* 0x440 */ s8*                field_440;
    /* 0x444 */ GpRec18            field_444;
    /* 0x45C */ Actor01600Range    ranges[8];
    /* 0x49C */ MATRIX             field_49C;
    /* 0x4BC */ s32                field_4BC;
    /* 0x4C0 */ s32                field_4C0;
    /* 0x4C4 */ s32                field_4C4;
    /* 0x4C8 */ byte               pad_4C8[4];
    /* 0x4CC */ s16                field_4CC;
    /* 0x4CE */ byte               pad_4CE[6];
    /* 0x4D4 */ Task*              field_4D4;
    /* 0x4D8 */ s16                field_4D8;
    /* 0x4DA */ s16                field_4DA;
    /* 0x4DC */ s16                field_4DC;
    /* 0x4DE */ byte               pad_4DE[2];
    /* 0x4E0 */ s32                field_4E0;
    /* 0x4E4 */ s32                field_4E4;
    /* 0x4E8 */ s16                field_4E8;
    /* 0x4EA */ s16                field_4EA;
    /* 0x4EC */ s16                field_4EC;
    /* 0x4EE */ s16                field_4EE;
    /* 0x4F0 */ s16                field_4F0;
    /* 0x4F2 */ s16                field_4F2;
    /* 0x4F4 */ u16                field_4F4;
    /* 0x4F6 */ s16                field_4F6;
    /* 0x4F8 */ s16                field_4F8;
    /* 0x4FA */ s16                field_4FA;
    /* 0x4FC */ s16                field_4FC;
    /* 0x4FE */ s16                field_4FE;
    /* 0x500 */ s16                field_500;
    /* 0x502 */ s16                field_502;
    /* 0x504 */ s16                field_504;
    /* 0x506 */ s16                field_506;
    /* 0x508 */ s16                field_508;
    /* 0x50A */ s16                field_50A;
    /* 0x50C */ s16                field_50C;
    /* 0x50E */ s16                field_50E;
    /* 0x510 */ s16                field_510;
    /* 0x512 */ s16                field_512;
    /* 0x514 */ s16                field_514;
    /* 0x516 */ s16                field_516;
    /* 0x518 */ s16                field_518;
    /* 0x51A */ s16                field_51A;
    /* 0x51C */ s16                field_51C;
    /* 0x51E */ s16                field_51E;
    /* 0x520 */ s16                field_520;
    /* 0x522 */ s16                field_522;
    /* 0x524 */ s16                field_524;
    /* 0x526 */ s16                field_526;
    /* 0x528 */ s16                field_528;
    /* 0x52A */ s16                field_52A;
    /* 0x52C */ s16                field_52C;
    /* 0x52E */ s16                field_52E;
    /* 0x530 */ s16                field_530;
    /* 0x532 */ s16                field_532;
    /* 0x534 */ s16                field_534;
    /* 0x536 */ s16                field_536;
    /* 0x538 */ s16                field_538;
    /* 0x53A */ s16                field_53A;
    /* 0x53C */ s16                field_53C;
    /* 0x53E */ s16                field_53E;
    /* 0x540 */ s16                field_540;
    /* 0x542 */ u16                field_542;
    /* 0x544 */ s16                field_544;
    /// Second animation id the `variant == 2` and `variant == 4` paths of
    /// `Actor01600_Fn05F80` run their countdown against: it is stored into
    /// `field_506` and steps 7 -> 9.
    /* 0x546 */ s16 field_546;
    /* 0x548 */ u16 field_548;
    /// Copy of the spawn variant `Actor01600_Fn05F80` takes its `case 0x1A`
    /// path for.
    /* 0x54A */ u16  field_54A;
    /* 0x54C */ u16  field_54C;
    /* 0x54E */ s16  field_54E;
    /* 0x550 */ s16  field_550;
    /* 0x552 */ byte pad_552[2];
    /* 0x554 */ s16  field_554;
    /* 0x556 */ s16  field_556;
} Actor01600Work;
STATIC_ASSERT_SIZEOF(Actor01600Work, 0x558);

typedef struct Actor01600 {
    /* 0x00 */ byte               pad_0[0xC];
    /* 0x0C */ struct Actor01600* field_C;
    /* 0x10 */ struct Actor01600* field_10;
    /* 0x14 */ byte               pad_14[4];
    /* 0x18 */ void               (*field_18)(struct Actor01600*);
    /* 0x1C */ Actor01600Work*    field_1C;
    /* 0x20 */ Actor01600Ctx*     field_20;
    /* 0x24 */ void*              field_24;
    /* 0x28 */ byte               pad_28[4];
    /* 0x2C */ TmdObject*         field_2C;
    /* 0x30 */ s32                field_30;
} Actor01600;
STATIC_ASSERT_SIZEOF(Actor01600, 0x34);

/// State table the overlay dispatches through, indexed by the actor's
/// `field_30`. Its three words are `Actor01600_Fn001F4`, `Actor01600_Fn00674`
/// and `Actor01600_Fn04054`, and every handler takes the actor's 0x20 context
/// first, like `Actor100300StateFuncTable3`. The object sits in the header unit
/// right after the overlay id, so `Actor01600_D00000` is that id and this table
/// starts at 0x4.
typedef struct Actor01600StateFuncTable3 {
    void (*funcs[3])(Actor01600Ctx*, Actor01600*);
} Actor01600StateFuncTable3;
STATIC_ASSERT_SIZEOF(Actor01600StateFuncTable3, 0xC);

extern Actor01600StateFuncTable3 Actor01600_D00004;

/// Overlay-local view of the gameplay `Gp_StateF0` block (`GpStateF0` in
/// `include/gameplay/3A34.h`). Flags are accessed as a word and individual
/// bytes; `field_1C` is read with `lb`, so it is signed here.
typedef struct Actor01600StateF0 {
    /* 0x00 */ union {
        u32 word;
        u8  bytes[4];
    } flags;
    /* 0x04 */ u8   field_4;
    /* 0x05 */ byte pad_5[0x17];
    /* 0x1C */ s8   field_1C;
    /* 0x1D */ byte pad_1D[0xF];
} Actor01600StateF0;
STATIC_ASSERT_SIZEOF(Actor01600StateF0, 0x2C);

/// Overlay-local view of the gameplay `Gp_StateC08` block (`GpStateC08` in
/// `include/gameplay/gameplay.h`).
typedef struct Actor01600StateC08 {
    /* 0x00 */ byte pad_0[6];
    /* 0x06 */ u8   field_6;
    /* 0x07 */ byte pad_7[3];
    /* 0x0A */ s8   field_A;
    /* 0x0B */ byte pad_B[0xD];
} Actor01600StateC08;
STATIC_ASSERT_SIZEOF(Actor01600StateC08, 0x18);

extern Actor01600StateF0  Gp_StateF0;
extern Actor01600StateC08 Gp_StateC08;

void Gp_DestroyEnemy(void* enemy, void* task);

/// Overlay-local view of the player state selected through Gp_ActorSlots.
typedef struct Actor01600PlayerState {
    /* 0x000 */ byte pad_0[0x954];
    /* 0x954 */ u16  field_954;
} Actor01600PlayerState;

typedef struct Actor01600PlayerSlot {
    /* 0x00 */ byte                   pad_0[0x1C];
    /* 0x1C */ Actor01600PlayerState* actor;
    /* 0x20 */ byte                   pad_20[0xC];
    /* 0x2C */ TmdObject*             field_2C;
} Actor01600PlayerSlot;

typedef struct Actor01600RotScratch {
    /* 0x00 */ VECTOR  position;
    /* 0x10 */ SVECTOR rotation;
} Actor01600RotScratch;
STATIC_ASSERT_SIZEOF(Actor01600RotScratch, 0x18);

/// 0x7C-byte scratch from `G_SCRATCH_HEAD` that `Actor01600_Fn045A8` aims from:
/// `delta` takes the world-space offset from the actor to the player, `dir` the
/// same offset written as an `SVECTOR` and then replaced by that offset turned
/// into the actor's own frame, and `mat` the transpose of the actor's rotation
/// the turn multiplies by. Only `delta` is reached through the allocated
/// pointer; the two above it are written at negative offsets from the scratchpad
/// head, which is what keeps the head in a register of its own.
typedef struct Actor01600AimScratch {
    /* 0x00 */ byte    pad_0[0x20];
    /* 0x20 */ VECTOR  delta;
    /* 0x30 */ byte    pad_30[0xC];
    /* 0x3C */ SVECTOR dir;
    /* 0x44 */ byte    pad_44[0x18];
    /* 0x5C */ MATRIX  mat;
} Actor01600AimScratch;
STATIC_ASSERT_SIZEOF(Actor01600AimScratch, 0x7C);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `Actor01600_Fn04C64`: `vec`
/// takes (0, 0, `distance`), `mat` the yaw rotation `func_8004BFF8` builds from
/// the work block's `field_4EC`, and `out` the `vec` turned by it - the
/// displacement the actor keeps in `field_42C` / `field_430`.
typedef struct Actor01600YawScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ SVECTOR out;
    /* 0x10 */ MATRIX  mat;
} Actor01600YawScratch;
STATIC_ASSERT_SIZEOF(Actor01600YawScratch, 0x30);

/// `gte_rtv0` as the retail build emits it: the full `mvmva 1,0,0,3,0` word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// `GsCOORDINATE2.coord.t[]` seen as three unsigned halfwords, so
/// `Actor01600_Fn052C4` loads each world coordinate with `lhu`. The same
/// narrowing `GpCoordXZ` does for X and Z, extended to Y.
typedef struct Actor01600CoordPos {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  x;
    /* 0x1A */ byte pad_1A[2];
    /* 0x1C */ u16  y;
    /* 0x1E */ byte pad_1E[2];
    /* 0x20 */ u16  z;
    /* 0x22 */ byte pad_22[2];
} Actor01600CoordPos;
STATIC_ASSERT_SIZEOF(Actor01600CoordPos, 0x24);

/// Index of the `Gp_ActorSlots` actor nearer to `arg0`, or 0 when slot 0 is
/// empty (or slot 1 is at least as far). The distance is planar: the Y
/// difference is computed into the scratch vector but never enters the sum.
s32 Actor01600_Fn052C4(Actor01600* arg0);

/// Payload for message 0x3F8.
typedef struct Actor01600Msg3F8 {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ byte pad_8[0xC];
    /* 0x14 */ s32  field_14;
} Actor01600Msg3F8;
STATIC_ASSERT_SIZEOF(Actor01600Msg3F8, 0x18);

/// Position and rotation payload for message 0x3E9.
typedef struct Actor01600Msg3E9 {
    /* 0x00 */ VECTOR  position;
    /* 0x10 */ SVECTOR rotation;
} Actor01600Msg3E9;
STATIC_ASSERT_SIZEOF(Actor01600Msg3E9, 0x18);

extern Actor01600PlayerSlot* Gp_ActorSlots[];
extern Actor01600Msg3F8      Actor01600_D12878;
extern Actor01600Msg3E9      Actor01600_D12890;

extern s32 Actor01600_D12874;
extern s32 Actor01600_D127DC;
extern s32 Actor01600_D12870;

extern TaskDesc D_800626EC[];
extern u8       Actor01600_D0973C[];
extern u8       Actor01600_D09CFC[];
extern u8       Actor01600_D09EE0[];
extern SVECTOR  Actor01600_D12868;

void Actor01600_Fn070AC(Actor01600* arg0, Actor01600* arg1);

void Actor01600_Fn01420(Actor01600* arg0);
void Actor01600_Fn04054(Actor01600Ctx* arg0, Actor01600* arg1);
void Actor01600_Fn066E8(Actor01600* arg0);

s32 Actor01600_Fn045A8(Actor01600* arg0, s32* distance);
s32 Actor01600_Fn04C64(Actor01600* arg0, s32 distance, s32 angle);

u8 Actor01600_Fn04EB0(Actor01600* arg0);

/// Scratchpad storage for the actor's ground-quad position and rotated offset.
typedef struct Actor01600GroundScratch {
    /* 0x00 */ VECTOR3 pos;
    /* 0x0C */ s32     pad_C;
    /* 0x10 */ SVECTOR offset;
} Actor01600GroundScratch;
STATIC_ASSERT_SIZEOF(Actor01600GroundScratch, 0x18);

/// Word-wise view of a `MATRIX` used to splat an identity rotation: five
/// aligned stores instead of nine halfword ones, each word holding two adjacent
/// `m[][]` entries. Same shape as `Actor206100MatrixWords`.
typedef struct Actor01600MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor01600MatWords;

typedef union Actor01600Matrix {
    MATRIX             mat;
    Actor01600MatWords ident;
} Actor01600Matrix;
STATIC_ASSERT_SIZEOF(Actor01600Matrix, 0x20);

/// 0x3C-byte `G_SCRATCH_HEAD` block `Actor01600_Fn06974` steps the attachment
/// coordinate in: the step vector the coordinate's facing is rotated into, the
/// `SVECTOR` `Gfx_MatrixCol2` reads that facing into, the rotation
/// `func_8004BFF8` builds for the yaw and the yaw itself.
typedef struct Actor01600StepScratch {
    /* 0x00 */ VECTOR           move;
    /* 0x10 */ SVECTOR          dir;
    /* 0x18 */ Actor01600Matrix mat;
    /* 0x38 */ s16              yaw;
    /* 0x3A */ byte             pad_3A[2];
} Actor01600StepScratch;
STATIC_ASSERT_SIZEOF(Actor01600StepScratch, 0x3C);

/// 0x30-byte `G_SCRATCH_HEAD` block `Actor01600_Fn06880` squashes the attachment
/// coordinate with: an identity `mat` scaled per axis by `scale` and multiplied
/// into the coordinate's own rotation. The axis scales are 1.0, the work
/// block's decaying `field_518` and 1.0 again. Same shape as the
/// `ActorShared80135b58Scratch` the other actors' shrinking bodies borrow.
typedef struct Actor01600ScaleScratch {
    /* 0x00 */ Actor01600Matrix mat;
    /* 0x20 */ VECTOR           scale;
} Actor01600ScaleScratch;
STATIC_ASSERT_SIZEOF(Actor01600ScaleScratch, 0x30);

void Actor01600_Fn03A60(Actor01600* actor);
void Gp_ArmStateF0(s32 active);
s32  Gp_CountRec18Hi(GpRec18* rec, s32 mask);

typedef struct Actor01600MsgState {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor01600MsgState;
STATIC_ASSERT_SIZEOF(Actor01600MsgState, 0x10);

extern Actor01600MsgState Actor01600_D127D8;

s32  Actor01600_Fn047A0(Actor01600* actor);
s32  Actor01600_Fn04974(Actor01600* actor, s32 angle, s32 distance, s32 flags);
void Actor01600_Fn06974(Actor01600* actor, s32 distance);
s32  Actor01600_Fn06C1C(Actor01600* actor);
s32  Actor01600_Fn06C94(Actor01600* actor, s32 angle, s32 distance);
s32  Actor01600_Fn06D74(Actor01600* actor, s32 angle, s32 distance);

extern Actor01600Desc Actor01600_D09F0C;
extern u8             Actor01600_D127EC[], Actor01600_D127A4[];
void                  Actor01600_Fn05400(Actor01600* actor);
void                  Actor01600_Fn06EA4(Actor01600* actor);
void                  Gp_LinkNode(Actor01600Node* node);
void                  func_800B3F84(Actor01600AnimCtx* anim, void* data, TmdObject* obj, void* poses, Actor01600AnimSlot* slots);
void                  Gp_AnimResetSlot(Actor01600AnimCtx* anim, s32 slot, s32 active);
void                  Gp_IncStateF0Ref(s32 arg0);

struct _GpObj;
struct _GpU16Pair;
void                     Gp_LinkObj(s32 kind, struct _GpObj* obj);
void                     Gp_InitRec18Table(GpRec18* table, s32 count, s32 occupied);
s32                      Gp_PackPair(struct _GpU16Pair* pair, s32 index);
void                     func_8004BFF8(s16 angle, MATRIX* matrix);
extern struct _GpU16Pair Actor01600_D09F04;

#endif // ACTOR_101600_H
