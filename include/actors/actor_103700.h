#ifndef ACTOR_103700_H
#define ACTOR_103700_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-actor work block, reached as `(Actor103700Work*)task->work`.
///
/// Like actor 421600 this overlay keeps its own state in the `Task::work` slot
/// instead of a `TaskIdMap`, so that pointer field is *not* a `TaskIdMap` here.
/// The halfword `func_actor_103700_801350DC` cycles counts up and wraps to 0 once
/// it passes the caller's period, and the wrapped value indexes the bob table
/// below at `arg1 * 15 + field_25E`; the sibling `func_actor_103700_801347E0`
/// drives the same counter with the same table.
///
/// The mode halfwords around it are what the shared `ActorsShared80135318`
/// gates on: it does nothing until `field_24E` has run below 7 with the task in
/// state 1, and then either flags `field_250` or steps `field_24E` to 5 with the
/// state copied into `field_248`.
typedef struct Actor103700Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[6];
    /* 0x104 */ byte       poses[0x60]; // pose buffer, `func_800B3F84` arg3
    /* 0x164 */ MATRIX     colorMtx;
    /* 0x184 */ MATRIX     lightMtx;
    /* 0x1A4 */ GpObj      obj;
    /* 0x1C4 */ GpRec18    records[4];
    /* 0x224 */ GpEffArg   field_224; // hit-spark record for `func_800FDB18`
    /* 0x22C */ SVECTOR    field_22C;
    /* 0x234 */ SVECTOR    field_234;
    /* 0x23C */ SVECTOR    field_23C;
    /* 0x244 */ s16        field_244;
    /* 0x246 */ s16        field_246;
    /* 0x248 */ s16        field_248;
    /* 0x24A */ s16        field_24A;
    /* 0x24C */ s16        field_24C;
    /* 0x24E */ s16        field_24E;
    /* 0x250 */ s16        field_250;
    /* 0x252 */ s16        field_252;
    /* 0x254 */ s16        field_254;
    /* 0x256 */ u16        field_256;
    /* 0x258 */ u16        field_258;
    /* 0x25A */ s16        field_25A;
    /* 0x25C */ u16        field_25C;
    /* 0x25E */ u16        field_25E;
    /* 0x260 */ byte       pad_260[0x2];
    /* 0x262 */ s16        field_262;
    /* 0x264 */ s16        field_264;
    /* 0x266 */ s16        field_266;
    /* 0x268 */ s16        field_268;
    /* 0x26A */ s16        field_26A;
    /* 0x26C */ u16        field_26C;
} Actor103700Work;

/// Pose context the `D_801153F4` mode switch in `func_actor_103700_80134E24`
/// writes: `field_14` is cleared in mode 0 and set in mode 2.
typedef struct Actor103700Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor103700Ctx;

/// Payload of the 0x3F8 query `func_actor_103700_80134F50` sends the player
/// before it takes the hold; `field_14` is the range it asks for. The same
/// shape as `Actor510900Msg3F8` and `Actor400600Msg3F8`.
typedef struct Actor103700Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} Actor103700Msg3F8;
STATIC_ASSERT_SIZEOF(Actor103700Msg3F8, 0x18);

/// 0x2C-byte scratch from `G_SCRATCH_HEAD` used by `func_actor_103700_80134F50`:
/// the 0x3F8 query buffer followed by the `GpAnimArg` it sends as message 0x3FF.
typedef struct Actor103700HoldScratch {
    /* 0x00 */ Actor103700Msg3F8 query;
    /* 0x18 */ GpAnimArg         anim;
} Actor103700HoldScratch;
STATIC_ASSERT_SIZEOF(Actor103700HoldScratch, 0x2C);

/// Spawn argument reached through `Task::spawnArg2`: the high nibble of
/// `field_8` selects the sound bank `func_actor_103700_80133AB4` plays from.
typedef struct Actor103700Spawn {
    /* 0x00 */ byte                    pad_0[0x8];
    /* 0x08 */ u16                     field_8;
    /* 0x0A */ byte                    pad_A[0x32];
    /* 0x3C */ struct Actor103700Kind* field_3C;
} Actor103700Spawn;

/// Record `Actor103700Spawn::field_3C` points at; `field_F` indexes the
/// halfword table `D_actor_103700_80139D9C`.
typedef struct Actor103700Kind {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ u16  field_2; // variant; `/ 10` picks the spawn mode
    /* 0x4 */ byte pad_4[0x6];
    /* 0xA */ u16  field_A;
    /* 0xC */ byte pad_C[0x3];
    /* 0xF */ u8   field_F;
} Actor103700Kind;

extern u16 D_actor_103700_80139D9C[];

/// Halfword tables `func_actor_103700_801336E8` indexes by a 4-bit LCG draw:
/// the countdown seeded into `field_258` and `field_256`.
extern u16 D_actor_103700_80139D5C[];
extern u16 D_actor_103700_80139D7C[];

/// Pair `func_actor_103700_80133370` packs with `Gp_PackPair` for message 0x3F9.
extern GpU16Pair D_actor_103700_80139D28;

/// Halfword table `func_actor_103700_80133370` indexes by a 4-bit LCG draw.
extern s16 D_actor_103700_80139D3C[];

/// Halfword bob table, one row of 15 per `arg1`: the row runs
/// 0, 10, 19, 24, 25, 22, 15, 5, -5, -15, -22, -25 before returning to 0.
/// Every use reads it as a signed halfword through `lh` and adds it to a
/// coordinate's Y translation, so it is the amplitude of an idle bob.
extern s16 D_actor_103700_80139DB8[];

/// Halfword wave table `func_actor_103700_80135140` indexes by `field_25C`.
extern s16 D_actor_103700_80139DF4[];

/// 8-byte rise step: while `field_24C` is below `threshold` the Y and
/// forward displacements are spread over `steps` frames.
typedef struct Actor103700Rise {
    /* 0x0 */ s16 threshold;
    /* 0x2 */ s16 steps;
    /* 0x4 */ s16 dy;
    /* 0x6 */ s16 dist;
} Actor103700Rise;

extern Actor103700Rise D_actor_103700_80139E14[];
extern Actor103700Rise D_actor_103700_80139E4C[];

/// The enemy's pair source; the spawn stores it in `GpEnemy::field_50` and
/// seeds HP from its `field_4`, which retail addresses as its own label.
extern GpPairSrcE D_actor_103700_80139D2C;
extern u16        D_actor_103700_80139D30;

/// Animation data `func_800B3F84` loads, and the task's `field_24` table.
extern u8    D_actor_103700_80139F04[];
extern void* D_actor_103700_80139F28;

extern u32 Gp_LcgState;

void func_actor_103700_801350DC(Task* task, s32 arg1, s32 arg2);
void func_actor_103700_80135140(Task* task, s32 arg1);
void func_actor_103700_8013537C(Task* task);
s32  func_actor_103700_80134F50(Task* task);

/// Animation-set table handed to the player as the 0x3FF payload's `field_0`.
extern GpAnimSet* D_actor_103700_80139F1C[];

/// 0x58-byte scratch from `G_SCRATCH_HEAD` used by `func_actor_103700_8013224C`:
/// `delta` receives the `func_800E0C10` push-back and is then reused for each
/// record's offset, `normal` is its `VectorNormal`, and `dir` that normal
/// transformed by the grid's `workm`.
typedef struct Actor103700PushScratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ VECTOR         normal;
    /* 0x40 */ VECTOR         dir;
    /* 0x50 */ byte           pad_50[0x8];
} Actor103700PushScratch;
STATIC_ASSERT_SIZEOF(Actor103700PushScratch, 0x58);

/// Halfword table indexed by the low 7 bits of a hit id; 3 cancels the damage.
extern s16 D_actor_103700_80139E94[];

void func_actor_103700_8013224C(Task* task, TmdObject* arg1, s32 arg2);

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by `func_actor_103700_801347E0`:
/// the offset to the target and its `VectorNormalS`. That function never gives
/// the scratch back.
typedef struct Actor103700SteerScratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR normal;
} Actor103700SteerScratch;
STATIC_ASSERT_SIZEOF(Actor103700SteerScratch, 0x18);

#endif // ACTOR_103700_H
