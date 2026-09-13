#ifndef ACTOR_461800_H
#define ACTOR_461800_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Per-actor work block for the `actor_461800` overlay.
///
/// The spawn routine `func_actor_461800_80132390` allocates it with
/// `Mem_Calloc(0x4F8, 0)` and stores the pointer both here (in
/// `D_actor_461800_80143894`) and in the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` in this overlay -- so the size below is the
/// allocation and not a guess. Every other function in the overlay reaches the
/// block through the global.
///
/// The prefix is the layout `ActorsShared801324c8` reseeds: `anim` is the
/// animation context `func_800B4114` walks, `field_4B8` the animation id the
/// slots are seeded with, `field_4B6` the copy of it kept for change detection
/// and `field_4EC` the reset argument handed to `func_800B4114`. The tail
/// holds the two helper tasks `ActorsShared80132ecc` kills on teardown.
typedef struct Actor461800Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x462];
    /* 0x4B6 */ s16       field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16       field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ byte      pad_4BA[0x32];
    /* 0x4EC */ s16       field_4EC; // reset argument handed to `func_800B4114`
    /* 0x4EE */ byte      pad_4EE[0x2];
    /* 0x4F0 */ Task*     field_4F0; // first helper task the spawn starts
    /* 0x4F4 */ Task*     field_4F4; // second helper task
} Actor461800Work;
STATIC_ASSERT_SIZEOF(Actor461800Work, 0x4F8);

extern Actor461800Work* D_actor_461800_80143894;

/// Message payload the state handlers take as `Gp_DispatchMsg`'s `arg2`.
typedef struct Actor461800Msg {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor461800Msg;

s32 func_actor_461800_80132F20(Task* arg0, s32 arg1, Actor461800Msg* arg2, s32 arg3);

#endif
