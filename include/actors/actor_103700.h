#ifndef ACTOR_103700_H
#define ACTOR_103700_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-actor work block, reached as `(Actor103700Work*)task->idMap`.
///
/// Like actor 421600 this overlay keeps its own state in the `Task::idMap` slot
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
    /* 0x000 */ byte pad_0[0x248];
    /* 0x248 */ s16  field_248;
    /* 0x24A */ byte pad_24A[0x4];
    /* 0x24E */ s16  field_24E;
    /* 0x250 */ s16  field_250;
    /* 0x252 */ s16  field_252;
    /* 0x254 */ s16  field_254;
    /* 0x256 */ byte pad_256[0x8];
    /* 0x25E */ u16  field_25E;
    /* 0x260 */ byte pad_260[0x2];
    /* 0x262 */ s16  field_262;
    /* 0x264 */ byte pad_264[0x2];
    /* 0x266 */ s16  field_266;
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
    /* 0x0 */ byte pad_0[0x8];
    /* 0x8 */ u16  field_8;
} Actor103700Spawn;

/// Halfword bob table, one row of 15 per `arg1`: the row runs
/// 0, 10, 19, 24, 25, 22, 15, 5, -5, -15, -22, -25 before returning to 0.
/// Every use reads it as a signed halfword through `lh` and adds it to a
/// coordinate's Y translation, so it is the amplitude of an idle bob.
extern s16 D_actor_103700_80139DB8[];

/// Animation-set table handed to the player as the 0x3FF payload's `field_0`.
extern GpAnimSet* D_actor_103700_80139F1C[];

#endif // ACTOR_103700_H
