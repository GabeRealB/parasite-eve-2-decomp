#ifndef ACTOR_206100_H
#define ACTOR_206100_H

#include "common.h"

/// Per-actor state block for the `actor_206100` overlay's enemy.
///
/// `func_actor_206100_8014C274` allocates it with `Mem_Calloc(0x558, 0)` and
/// stores it straight into the `Task::idMap` slot (0x1C), so the size below is
/// the allocation and not a guess: this overlay reuses that pointer field for
/// its own work block and it is *not* a `TaskIdMap` here.  Reach it with
/// `(Actor206100Work*)task->idMap`.  (The overlay's only other allocation,
/// `Mem_Calloc(0x68, 0)` in `func_actor_206100_8014C458`, belongs to the child
/// task that `Task_SpawnFromTable` returns there, so it is a different `Task`
/// and a different block.)
///
/// `field_520` / `field_522` are the state and sub-state indices the handler
/// table walks and `field_51E` is the per-state frame counter -- the same
/// layout the other enemy overlays use.  `field_50C` .. `field_51A` are the
/// animation request the actor hands to its player: `func_actor_206100_8014C274`
/// writes `field_50C` as the request kind and then reads `field_50E` and
/// `field_510` as the clip to play, with `field_51A` the step scale.
typedef struct Actor206100Work {
    /* 0x000 */ byte pad_0[0x50C];
    /* 0x50C */ s16  field_50C; // animation request kind
    /* 0x50E */ byte pad_50E[0x2];
    /* 0x510 */ s16  field_510; // animation clip id
    /* 0x512 */ byte pad_512[0x8];
    /* 0x51A */ s16  field_51A; // animation step scale
    /* 0x51C */ byte pad_51C[0x2];
    /* 0x51E */ u16  field_51E; // per-state frame counter
    /* 0x520 */ s16  field_520; // state index
    /* 0x522 */ u16  field_522; // sub-state index
    /* 0x524 */ s16  field_524;
    /* 0x526 */ u16  field_526;
    /* 0x528 */ byte pad_528[0xE];
    /* 0x536 */ u16  field_536; // seeded from D_80181A48 when the block is built
    /* 0x538 */ byte pad_538[0xC];
    /* 0x544 */ s16  field_544; // id handed to func_actor_206100_8014EB48
    /* 0x546 */ byte pad_546[0xE];
    /* 0x554 */ s8   field_554;
    /* 0x555 */ byte pad_555[0x1];
    /* 0x556 */ s8   field_556;
    /* 0x557 */ byte pad_557[0x1];
} Actor206100Work;
STATIC_ASSERT_SIZEOF(Actor206100Work, 0x558);

#endif
