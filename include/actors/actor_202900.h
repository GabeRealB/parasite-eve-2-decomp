#ifndef ACTOR_202900_H
#define ACTOR_202900_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"

/// Per-actor work block for the `actor_202900` overlay, reached through the
/// global `ActorsShared80131f9c` publishes.
///
/// Only the prefix the overlay's matched functions need is reconstructed so
/// far. `anim` sits at 0x40 and the slot array at 0x54, the same layout as
/// `Actor143900Work` and `Actor151000Work`; the nineteen slots are the ones
/// `func_actor_202900_8014A208` ticks.
typedef struct Actor202900Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       pad_34C[0x130];
    /* 0x47C */ s16        field_47C; // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x47E */ byte       pad_47E[2];
    /* 0x480 */ u16        animId;    // animation id the slots are seeded with
    /* 0x482 */ s16        field_482; // cleared when a step body is started
} Actor202900Work;

extern Actor202900Work* ActorsShared80131f9cWork;

/// Argument block of the message handler `func_actor_202900_8014A3E0`
/// implements: which animation to start. Same 4-byte-id prefix as
/// `Actor110300AnimArgs` and `Actor460200AnimArgs`, and the same `(u16)`
/// narrowing on the store into the work block's `animId`.
typedef struct Actor202900AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
} Actor202900AnimArgs;

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. `arg0` is handed the actor but the body ignores it: it
/// reaches the work block through the global, like every other function in the
/// overlay.
void func_actor_202900_8014A194(GpActorWork* arg0);
void func_actor_202900_8014A208(void);
void func_actor_202900_8014A260(void);
void func_actor_202900_8014A304(void);
s32  func_actor_202900_8014A3E0(Task* task, s32 arg1, Actor202900AnimArgs* args);

#endif
