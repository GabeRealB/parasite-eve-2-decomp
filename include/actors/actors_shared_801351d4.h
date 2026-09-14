#ifndef ACTORS_SHARED_801351D4_H
#define ACTORS_SHARED_801351D4_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. `func_actor_102400_801345B0`
/// allocates it with `Mem_Calloc(0xB4, 0)`, so the size below is the
/// allocation, not a guess.
///
/// The three `GpObj` nodes are the display objects the teardown
/// `ActorsShared801351d4` hands back to `Gp_UnlinkObj`; the one at +0x58 is the
/// node `func_actor_102400_801345B0` passes to `Gp_LinkObj`. `field_B0` is the
/// death timer, armed to 0x5A on spawn and counted down by
/// `func_actor_102400_80134910`; `field_B2` is the sub-state the teardown
/// switches on and `func_actor_102400_80134910` clears.
typedef struct ActorsShared801351d4Work {
    /* 0x00 */ GpObj   obj_0;
    /* 0x20 */ GpObj   obj_20;
    /* 0x40 */ u16     field_40; // bit 0 forces the death effect to spawn
    /* 0x42 */ byte    pad_42[0x16];
    /* 0x58 */ GpObj   obj_58;
    /* 0x78 */ byte    pad_78[0x18];
    /* 0x90 */ GpRec18 field_90; // collision record wiped on every tick
    /* 0xA8 */ s16     field_A8; // per-axis step added to the coord below
    /* 0xAA */ byte    pad_AA[0x2];
    /* 0xAC */ s16     field_AC;
    /* 0xAE */ byte    pad_AE[0x2];
    /* 0xB0 */ u16     field_B0; // death timer
    /* 0xB2 */ s16     field_B2; // sub-state
} ActorsShared801351d4Work;
STATIC_ASSERT_SIZEOF(ActorsShared801351d4Work, 0xB4);

/// Teardown for the actor: state 0 unlinks the three display nodes and arms
/// the death timer, state 1 counts it down and destroys the enemy once it
/// reaches zero.
///
/// Shared by `actor_102400` and `actor_202400`.
void ActorsShared801351d4(GpEnemy* arg0, Task* arg1);

#endif
