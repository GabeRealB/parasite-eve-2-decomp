#ifndef ACTOR_461800_H
#define ACTOR_461800_H

#include "common.h"

#include "actors/actors_shared_8013411c.h"
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
    /* 0x4BA */ byte      pad_4BA[0x2C];
    /* 0x4E6 */ s16       field_4E6; // yaw seeding the root coordinate
    /* 0x4E8 */ byte      pad_4E8[0x4];
    /* 0x4EC */ s16       field_4EC; // reset argument handed to `func_800B4114`
    /* 0x4EE */ byte      pad_4EE[0x2];
    /* 0x4F0 */ Task*     field_4F0; // first helper task the spawn starts
    /* 0x4F4 */ Task*     field_4F4; // second helper task
} Actor461800Work;
STATIC_ASSERT_SIZEOF(Actor461800Work, 0x4F8);

extern Actor461800Work* D_actor_461800_80143894;

/// Work block of the overlay's second actor variant. `func_actor_461800_8013307C`
/// allocates it with `Mem_Calloc(0x4C0, 0)` and stores the pointer in
/// `D_actor_461800_801438A0` and in the task's `Task::idMap` slot, the same pair
/// of places the first variant's block goes -- so the size below is the
/// allocation, not a guess. The animation context sits at the same 0x40, but
/// this variant keeps its change-detection pair at 0x47E / 0x480 where
/// `Actor461800Work` has 0x4B6 / 0x4B8.
typedef struct Actor461800Work2 {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x428];
    /* 0x47C */ s16       field_47C; // 1 or 2, picked by the message at 80133898
    /* 0x47E */ s16       field_47E; // copy of `field_480`, kept for change detection
    /* 0x480 */ s16       field_480; // animation id the slots are seeded with
    /* 0x482 */ s16       field_482;
    /* 0x484 */ byte      pad_484[0x2A];
    /* 0x4AE */ s16       field_4AE;
    /* 0x4B0 */ byte      pad_4B0[0x4];
    /* 0x4B4 */ s16       field_4B4; // reset argument the message handler writes
    /* 0x4B6 */ byte      pad_4B6[0x2];
    /* 0x4B8 */ s32       field_4B8; // only ever cleared, before the seeding loop
    /* 0x4BC */ u8        field_4BC;
    /* 0x4BD */ byte      pad_4BD[0x3];
} Actor461800Work2;
STATIC_ASSERT_SIZEOF(Actor461800Work2, 0x4C0);

extern Actor461800Work2* D_actor_461800_801438A0;

/// Message payload the state handlers take as `Gp_DispatchMsg`'s `arg2`.
typedef struct Actor461800Msg {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor461800Msg;

void func_actor_461800_80132390(GpEnemy* enemy, Task* task);
void func_actor_461800_80132A0C(GpEnemy* enemy, Task* task);
void func_actor_461800_8013307C(GpEnemy* enemy, Task* task);
void func_actor_461800_801335B0(GpEnemy* enemy, Task* task);

s32 func_actor_461800_80133970(Task* task, s32 arg1, ActorShared8013411cPlacement* placement);
s32 func_actor_461800_80132EA4(Task* task, s32 arg1, ActorShared8013411cPlacement* placement);
s32 func_actor_461800_80132F20(Task* arg0, s32 arg1, Actor461800Msg* arg2, s32 arg3);
s32 func_actor_461800_801339EC(Task* task, s32 arg1, Actor461800Msg* msg, s32 arg3);

#endif
