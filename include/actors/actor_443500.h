#ifndef ACTOR_443500_H
#define ACTOR_443500_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

#include "gameplay/1BC.h"

/// Position context the spawn handler reads through `Task::spawnArg2` (0x20):
/// the halfword at 0x8 packs the area position, shifted down by 12 into the
/// index `Gp_GetNestedAreaRec`'s table is walked with.
typedef struct Actor443500Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
} Actor443500Ctx;

/// Work block `func_actor_443500_80132078` `Mem_Calloc`s (0x4C4) and parks in
/// the task's `Task::idMap` slot (0x1C) -- that slot is not a `TaskIdMap`
/// here, just as with `Actor335800Work` and the other sharers of
/// `ActorsShared80132f24`. The spawn handler seeds the two `sb` bytes at
/// 0x475/0x476 and the word at 0x4BC to -1 and copies the parent TmdObject's
/// flags halfword to 0x4C0; the light/colour matrix pair at 0x478/0x498 is the
/// one `ActorsShared80132f24` republishes onto the model.
///
/// The size is the allocation; the fields below are the ones this overlay's
/// decompiled bodies touch.
typedef struct Actor443500Work {
    /* 0x000 */ GpAnimCtx anim;
    /// The twenty slots `func_actor_443500_801327E0` walks 1..0x13, each
    /// 0x28 bytes, filling the span up to the matrix table.
    /* 0x014 */ GpAnimSlot slots[0x14];
    /// The matrix table `func_800B3F84` fills, immediately after the slots.
    /* 0x334 */ byte field_334[0x140];
    /// Raised by `func_actor_443500_801327E0` after its last slot pass, and
    /// the condition its start branch reads: the first pass, while it is still
    /// clear, clears the slots instead.
    /* 0x474 */ s8     field_474;
    /* 0x475 */ s8     field_475;
    /* 0x476 */ s8     field_476;
    /* 0x477 */ byte   pad_477[0x1];
    /* 0x478 */ MATRIX light;
    /* 0x498 */ MATRIX color;
    /* 0x4B8 */ byte   pad_4B8[0x2];
    /// Cleared by `func_actor_443500_801327E0` after the slot passes, beside
    /// the `field_474` latch it raises.
    /* 0x4BA */ s16 field_4BA;
    /* 0x4BC */ s32 field_4BC;
    /* 0x4C0 */ s32 field_4C0;
} Actor443500Work;
STATIC_ASSERT_SIZEOF(Actor443500Work, 0x4C4);

/// The bank table `func_actor_443500_801327E0` re-seeds the work block's slot
/// array off: one entry, the animation bank the default preset's `field_0` of
/// zero selects.
extern void* D_actor_443500_80158724[];

/// Per-frame animation setter for the 0x7D3 opcode, the twenty-slot twin of
/// `func_actor_335800_801632A4`: re-seeds the slot array off the bank table
/// when the preset's bank index changes, then starts or resets every slot and
/// ticks them, latching `field_474`. Built by `actor_443500_2`.
s32 func_actor_443500_801327E0(Task* task, s32 anim, GpAnimArg* params, s32 arg3);

#endif // ACTOR_443500_H
