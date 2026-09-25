#ifndef ACTOR_202900_H
#define ACTOR_202900_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"

/// Per-actor work block for the `actor_202900` overlay, reached through the
/// global `D_actor_202900_80156E54`, which the actor's update
/// `func_actor_202900_8014A02C` refreshes from the task every frame.
///
/// The spawn handler allocates it at 0x564 bytes. It opens with the light and
/// colour matrices the actor's model is drawn under, then the animation context
/// with one slot and one 0x10-byte pose record for each of the nineteen parts
/// `func_actor_202900_8014A208` ticks: the same layout as `Actor143900Work`
/// and `Actor151000Work`. Only the fields the matched functions reach are
/// reconstructed after that.
typedef struct Actor202900Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       poses[0x13][0x10];
    /* 0x47C */ s16        field_47C; // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x47E */ u16        field_47E; // animation id currently playing
    /* 0x480 */ u16        animId;    // animation id the slots are seeded with
    /* 0x482 */ s16        field_482; // cleared when a step body is started
    /* 0x484 */ s16        field_484; // frame the second slot last held when it was 0x15, kept for change detection
} Actor202900Work;

/// The actor's work block, published so the overlay's functions can reach it
/// without the task in hand.
extern Actor202900Work* D_actor_202900_80156E54;

/// The actor's task, published by the spawn handler so the overlay's other
/// functions can reach the actor's model without the task in hand.
extern GpActorWork* D_actor_202900_80156E58;

/// The second task the spawn handler starts. Its model is textured from the
/// area record the actor was placed from, shown and hidden together with the
/// actor's, and the task is killed when the actor's exit callback runs.
extern Task* D_actor_202900_80156E5C;

/// Argument block of the message handler `func_actor_202900_8014A3E0`
/// implements: which animation to start. Same 4-byte-id prefix as
/// `Actor110300AnimArgs` and `Actor460200AnimArgs`, and the same `(u16)`
/// narrowing on the store into the work block's `animId`.
typedef struct Actor202900AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
} Actor202900AnimArgs;

void func_actor_202900_80149E24(GpEnemy* enemy, Task* task);
void func_actor_202900_8014A0B4(GpEnemy* enemy, Task* task);
/// Exit callback: kills the second task and destroys the enemy.
void func_actor_202900_8014A158(Task* arg0);
/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. `arg0` is handed the actor but the body ignores it: it
/// reaches the work block through the global, like every other function in the
/// overlay.
void func_actor_202900_8014A194(GpActorWork* arg0);
void func_actor_202900_8014A208(void);
void func_actor_202900_8014A260(void);
void func_actor_202900_8014A304(void);
s32  func_actor_202900_8014A394(void);
s32  func_actor_202900_8014A3E0(Task* task, s32 arg1, Actor202900AnimArgs* args);
/// Draw-state handler for message 0x7D5: bit 0 of `flags` selects visible or
/// hidden, bit 1 marks the colour matrix for a rebuild.
s32 func_actor_202900_8014A440(Task* task, s32 arg1, s32 flags);

#endif
