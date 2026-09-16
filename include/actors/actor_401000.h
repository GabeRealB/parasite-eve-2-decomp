#ifndef ACTOR_401000_H
#define ACTOR_401000_H

#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// Private work block of the actor 401000 task, hanging off `Task::idMap`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` display nodes the teardown hands
/// back to `Gp_UnlinkObj`, and the two child tasks it kills. The nodes sit
/// 8 bytes later than the 0x8C8/0xA08/0xB48 triple on actor 01900/401800, with
/// the same 0x140 stride.
/// `field_4` is the live-actor flag `func_actor_401000_8013DB10` tests, and
/// `field_B50.flags` / `field_A10.flags` are the two masks it writes. The
/// halfwords at 0x898..0x8A2 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes; `func_actor_401000_8013DB6C` is that body
/// with `field_A10.flags |= 0x4000` in place of the sibling's `&= 0xBFFF`.
typedef struct Actor401000Work {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// One-shot latch `func_actor_401000_8013922C` raises once the actor's
    /// spawn sound has been queued; the same slot `Actor401300Work` keeps at
    /// +0x6.
    /* 0x006 */ s16  field_6;
    /* 0x008 */ byte pad_8[0x52];
    /* 0x05A */ u16  field_5A;
    /* 0x05C */ byte pad_5C[0xC];
    /* 0x068 */ u16  field_68;
    /* 0x06A */ byte pad_6A[0x82A];
    /* 0x894 */ s32  field_894;
    /* 0x898 */ s16  field_898;
    /* 0x89A */ s16  field_89A;
    /* 0x89C */ byte pad_89C[2];
    /* 0x89E */ s16  field_89E;
    /* 0x8A0 */ byte pad_8A0[2];
    /* 0x8A2 */ s16  field_8A2;
    /* 0x8A4 */ s16  field_8A4;
    /* 0x8A6 */ byte pad_8A6[8];
    /* 0x8AE */ s16  field_8AE;
    /* 0x8B0 */ s16  field_8B0;
    /* 0x8B2 */ byte pad_8B2[2];
    /// Last animation state `func_actor_401000_8013922C` acted on; the same
    /// de-duplication slot `Actor401300Work` keeps at +0x8BC.
    /* 0x8B4 */ s32      field_8B4;
    /* 0x8B8 */ GpEffArg field_8B8;
    /// Offset the actor's state-3/5/7/8 effects spawn at, passed as the
    /// `Gp_SpawnEff` position: the same local `SVECTOR` `Actor401300` keeps on
    /// the stack for the 3013B6E8 triple, materialised into the work block
    /// here because every one of the four spawns reads it.
    /* 0x8C0 */ SVECTOR field_8C0;
    /* 0x8C8 */ byte    pad_8C8[8];
    /* 0x8D0 */ GpObj   field_8D0;
    /// The two obstacle-record tables `func_actor_401000_801323EC` slides the
    /// root coordinate against; the same pair `Actor01900Work` keeps at
    /// +0x8E8 / +0xA28 and `func_actor_401300_801323B0` walks at +0x990/+0xAD0.
    /* 0x8F0 */ byte  field_8F0[0x120];
    /* 0xA10 */ GpObj field_A10;
    /* 0xA30 */ byte  field_A30[0x120];
    /* 0xB50 */ GpObj field_B50;
    /* 0xB70 */ byte  pad_B70[0xA6];
    /// Radius `func_actor_401000_8013922C` tests the actor's distance from
    /// `D_80073B8C` against.
    /* 0xC16 */ u16 field_C16;
    /// The three bytes `func_actor_401000_8013D958` copies out of the front of
    /// the message payload; the same triple `Actor01900Work` keeps at +0xC34.
    /* 0xC18 */ u8   field_C18[3];
    /* 0xC1B */ byte pad_C1B;
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xC1C */ Task* field_C1C;
    /* 0xC20 */ Task* field_C20;
} Actor401000Work;

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::idMap` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900`.
typedef struct Actor401000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor401000Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor401000;

/// Message payload of `func_actor_401000_8013D694`, the actor's animation
/// request handler: `field_4` is the requested clip index, which the handler
/// maps onto `Actor401000Work.field_89E` (0x22-0x25, 0x27) and then restarts
/// the state halfwords. Only the two leading words of the argument block are
/// read, so the struct covers just those.
typedef struct Actor401000Msg {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} Actor401000Msg;

/// Height-clamp row `func_actor_401000_801352DC` scans: `field_0` / `field_2`
/// are matched against `GameSessionFrom4::field_3` / `field_2`, and when a row
/// matches the coordinate's Y is clamped to [`lo`, `hi`] and its `flg` is
/// cleared so the local matrix is rebuilt. Two rows
/// (`(1, 3, -0x12C, 0)`, `(5, 0x1D, 0, 0x12C)`). Same shape as
/// `Actor401300HeightClamp` / `Actor01900HeightClamp`.
typedef struct Actor401000HeightClamp {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ s16  lo;
    /* 0x6 */ s16  hi;
    /* 0x8 */ byte pad_8[8];
} Actor401000HeightClamp;
STATIC_ASSERT_SIZEOF(Actor401000HeightClamp, 0x10);

/// 0xC-byte scratch `func_actor_401000_8013922C` carves off `G_SCRATCH_HEAD`
/// to hold the actor's XZ offset from `D_80073B8C` and the radius it is
/// measured against, each squared before the comparison. Same shape as
/// `Actor01900RangeScratch` / `Actor401300RangeScratch`.
typedef struct Actor401000RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor401000RangeScratch;
STATIC_ASSERT_SIZEOF(Actor401000RangeScratch, 0xC);

extern MATRIX* D_80073B8C;

/// Overlay-data word `func_actor_401000_8013922C` points
/// `D_actor_401000_80154E88` at on entering its state.
extern s32  D_actor_401000_80154634;
extern s32* D_actor_401000_80154E88;

/// Message 0x3FF payload of `func_actor_401000_801383F0` and
/// `func_actor_401000_801385B0`: the animation argument the player task reads
/// when the actor's live-actor flag goes up.
extern GpAnimArg D_actor_401000_80154F1C;

/// The two `Actor401000HeightClamp` rows `func_actor_401000_801352DC` and
/// `func_actor_401000_80135374` walk.
extern Actor401000HeightClamp D_actor_401000_80154FD0[];

/// Message 0x3E9 payload of `func_actor_401000_801380B8`: the player task's
/// world position, then the yaw from the actor to it, handed straight to the
/// slot-3 handler. The 401000 twin of the block `func_actor_401300_80138800`
/// keeps inline at `Actor401300Work.field_CD4` / `.field_CE4`.
extern GpMsg3EE D_actor_401000_80155018;

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401000_8013B1E4`.
extern void* D_80114B78[1];

/// Overlay effect model data `func_actor_401000_8013B1E4` points
/// `D_80114B78` at before spawning, one per animation-latch key frame
/// (`field_6` 3, 5, 7, 8).
extern char D_actor_401000_80143EB4;
extern char D_actor_401000_80144830;
extern char D_actor_401000_8014599C;
extern char D_actor_401000_80146190;

void func_actor_401000_80132EF0(Actor401000* arg0);

/// Walk a `GpRec18` table and push `coord` back out of the obstacles it
/// overlaps, returning the record's `field_10`. The same helper as
/// `func_actor_401300_801323B0`, whose second argument is the sibling's
/// `field_990` run.
s32 func_actor_401000_801323EC(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2);

s32 func_actor_401000_8013D694(Actor401000* arg0, s32 arg1, Actor401000Msg* arg2);

/// Message 0x301 / 0x1002 handler: copy the payload's three leading bytes onto
/// the work block's animation slots, then key the actor's state (`field_0`) off
/// the message id and sub-id. The 0x1002/2 arm also drops the model root to
/// `(-0x595, 0, -0x5B1)` and rebuilds its yaw. Same body as
/// `Actor01900_Fn0A5A4`, minus that one's 0x301/0 arm.
s32 func_actor_401000_8013D958(Actor401000* arg0, s32 arg1, u16* arg2);

/// `Task::exitCallback` teardown: kill the two helper tasks, unlink the three
/// display nodes, drop the enemy's `field_54` slot, then `Gp_DestroyEnemy`.
void func_actor_401000_8013DA78(Task* task);
void func_actor_401000_8013DB10(Actor401000* arg0);
void func_actor_401000_8013DB6C(Actor401000* arg0);
void func_actor_401000_8013DC14(Actor401000* arg0);
void func_actor_401000_8013DCC0(Actor401000* arg0);
void func_actor_401000_8013DD6C(Actor401000* arg0);
void func_actor_401000_8013DEC8(Actor401000* arg0);

#endif // ACTOR_401000_H
