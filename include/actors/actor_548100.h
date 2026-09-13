#ifndef ACTOR_548100_H
#define ACTOR_548100_H

#include "common.h"

#include "main/task.h"

/// Per-instance work block of actor_548100, parked in `Task::idMap` -- that
/// slot is not a `TaskIdMap` here, it is the `Mem_Calloc(0x18, 0)` block
/// `func_actor_548100_80132420` allocates at spawn and stores at
/// `Task::idMap` (0x1C). Reach it with `(Actor548100Work*)task->idMap`.
///
/// `step` is the actor's 1-based progress step (0 while unset): the spawner
/// `func_actor_548100_80132550` seeds it from the `field_8` of the first
/// `D_actor_548100_801357E8` record whose `field_B` is set, and
/// `func_actor_548100_80132684` switches on it with nine cases. The game-flag
/// nibble recording whether a step is done is `step + 0xBE`, which is why the
/// same `+ 0xBE` shows up at every `GameFlag_GetNibble` / `GameFlag_SetNibble`
/// site in the overlay. `bit2Slot` is the 2-bit slot this instance occupies in
/// the current `Gp_Bit2Banks` word (`Gp_GetCurBit2Flag` / `Gp_SetCurBit2Flag`,
/// seeded with 5 or 4 by `func_actor_548100_80132684`) and `collectBitId` an id
/// in the `Gp_ClearCollectedBit` space (0x120 for the instance that reaches
/// `Gp_StartCapSlot` with kind 1).
///
/// 0x8 and up is the ramp `func_actor_548100_80134FEC` drives: `field_8` is the
/// period, `field_A` the elapsed counter it advances by 4 and clamps to
/// `field_8`, `field_C` the value that period ramps to, `field_E` the
/// interpolated result `field_C * field_A / field_8`, and `field_10` / `field_12`
/// a second period/elapsed pair on the same shape. The names stop at the last
/// field those bodies reach; the block is 0x18 bytes in full.
typedef struct Actor548100Work {
    /* 0x00 */ byte pad_0[0x2];
    /* 0x02 */ s16  step;
    /* 0x04 */ s16  collectBitId;
    /* 0x06 */ s8   field_6;
    /* 0x07 */ s8   bit2Slot;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ s16  field_A;
    /* 0x0C */ s16  field_C;
    /* 0x0E */ s16  field_E;
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ byte pad_14[0x4];
} Actor548100Work;
STATIC_ASSERT_SIZEOF(Actor548100Work, 0x18);

/// One record of the edge table `D_actor_548100_801351D0`: a directed link of
/// the stage graph this actor patrols and draws. `nodeA` / `nodeB` are node ids
/// (0-99) indexing the 4-byte point table `D_actor_548100_801358E4`
/// (`s16 x, y`); `func_actor_548100_80133684` reads both endpoints' points and
/// draws the segment between them. A node pair also keys the edge-id matrix
/// `D_actor_548100_80135B5C` as `prev * 100 + cur`, which is how the route walk
/// in `func_actor_548100_80134AE0` and `func_actor_548100_80134CB8` gets from a
/// step of the route string back to a record here: the bytes of
/// `D_actor_548100_80135B24[id]` are successive node ids, 0xFF-terminated.
///
/// The record is 14 bytes -- the stride `func_actor_548100_80134AE0` computes
/// as `id * 7 * 2` -- and only `nodeA`, `nodeB` and `field_2` are seeded in the
/// ROM; the rest is runtime state. `state` is the 1-based progress step the
/// drawing switch in `func_actor_548100_80133684` dispatches on (it subtracts 1
/// and accepts 0-4 as a case index): `func_actor_548100_80134AE0` writes 2 or 3
/// into it and `func_actor_548100_80134BF0` 0 or 1, the latter choosing between
/// them by comparing `field_2` with 2 (records 0-3 carry 2, records 73-78
/// carry 1). `flag_3` gates the direction branch of the drawing code, `dist` is
/// a per-segment value summed along a route by `func_actor_548100_80134CB8`,
/// and `field_C` a signed value that code scales by the segment's horizontal
/// direction.
///
/// `func_actor_548100_80134BA8` walks the table from its head and stops at the
/// first record whose `nodeA` is 0: an all-zero sentinel record, the 92nd, so 91
/// real records. The table's extent is 0x508 bytes, ending exactly where
/// `D_actor_548100_801356D8` begins -- only its leading 0x200 bytes are covered
/// by this symbol, the splitter having put the stray `D_actor_548100_801353D0`
/// label inside the array, mid-record.
typedef struct Actor548100Edge {
    /* 0x00 */ u8   nodeA;
    /* 0x01 */ u8   nodeB;
    /* 0x02 */ u8   field_2;
    /* 0x03 */ u8   flag_3;
    /* 0x04 */ byte pad_4[0x4];
    /* 0x08 */ u8   state;
    /* 0x09 */ byte pad_9[0x1];
    /* 0x0A */ s16  dist;
    /* 0x0C */ s16  field_C;
} Actor548100Edge;
STATIC_ASSERT_SIZEOF(Actor548100Edge, 0xE);

extern Actor548100Edge D_actor_548100_801351D0[];

#endif
