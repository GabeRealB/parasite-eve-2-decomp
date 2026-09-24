#ifndef ACTOR_213100_H
#define ACTOR_213100_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// Work block the spawn handler `func_actor_213100_8014A118` allocates
/// (`memCalloc(0x488)`) and parks in `Task::work` -- that slot is not a
/// `TaskIdMap` here.
///
/// It opens with the animation context the 0x7D3 handler
/// `func_actor_213100_8014A258` drives: the `GpAnimCtx` at the block's own
/// address, the 0x13 slots above it and the table at 0x30C, the three
/// arguments that handler hands `func_800B3F84`. `field_43C` latches once the
/// slots have been started, and gates the per-frame tick; `field_43E` and
/// `field_43D` hold the current bank index and animation id, seeded to -1 so
/// the first preset always installs. `light` / `color` are the matrices
/// `func_actor_213100_8014A23C` publishes on the model. `field_480` is the
/// child task the spawn handler creates, whose model mirrors this one's
/// visibility; `field_484` is the countdown after which the tick frees the
/// model's buffers, -1 while idle.
typedef struct Actor213100Work {
    /* 0x000 */ GpAnimCtx    anim;
    /* 0x014 */ GpAnimSlot   slots[0x13];
    /* 0x30C */ byte         field_30C[0x130];
    /* 0x43C */ s8           field_43C;
    /* 0x43D */ s8           field_43D;
    /* 0x43E */ s8           field_43E;
    /* 0x43F */ byte         pad_43F[0x1];
    /* 0x440 */ MATRIX       light;
    /* 0x460 */ MATRIX       color;
    /* 0x480 */ struct Task* field_480;
    /* 0x484 */ s32          field_484;
} Actor213100Work;
STATIC_ASSERT_SIZEOF(Actor213100Work, 0x488);

/// Animation preset the 0x7D3 handler `func_actor_213100_8014A258` takes;
/// the spawn handler builds one on its stack as `{ 0, 5, 0, 0, 0 }` and calls
/// the handler with it directly. `field_0` is the bank index into
/// `D_actor_213100_801521A4`, latched into `Actor213100Work::field_43E`;
/// `field_4` is the animation id, latched into `field_43D`; a nonzero
/// `field_8` installs the id through `func_800B4114`, which also takes
/// `field_C`, once the slots have been started. Nothing here reads
/// `field_10`; the size is the five words the spawn handler stores.
typedef struct Actor213100AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor213100AnimPreset;
STATIC_ASSERT_SIZEOF(Actor213100AnimPreset, 0x14);

void func_actor_213100_8014A21C(Task* arg0);

void func_actor_213100_8014A23C(Task* arg0);

s32 func_actor_213100_8014A258(Task* arg0, s32 arg1, Actor213100AnimPreset* arg2, s32 arg3);

#endif
