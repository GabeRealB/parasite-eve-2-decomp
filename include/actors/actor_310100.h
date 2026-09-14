#ifndef ACTOR_310100_H
#define ACTOR_310100_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Payload of message 0x7D4: the same world translation + Euler rotation record
/// the shared placement helpers consume, of which only the yaw (`rot.vy`) is
/// used here.
typedef ActorShared8013411cPlacement Actor310100Placement;

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. `func_actor_310100_801625E4` allocates it
/// with `Mem_Malloc(0x50C)` and hands `&slots` to the model helpers as the slot
/// array, so the prefix is the shared actor anim layout: a `GpAnimCtx` and the
/// nineteen slots the frame handler ticks.
typedef struct Actor310100Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[19];
    /* 0x30C */ byte       pad_30C[0x130];
    /// Light and colour matrices, handed to the model `TmdObject`'s `field_1C`
    /// and `field_20`.
    /* 0x43C */ MATRIX     field_43C;
    /* 0x45C */ MATRIX     field_45C;
    /* 0x47C */ byte       pad_47C[0x68];
    /* 0x4E4 */ Task*      field_4E4; // display task, killed and cleared by func_actor_310100_80162F34
    /* 0x4E8 */ Task*      field_4E8; // Game_GetPtrSlot(3)
    /* 0x4EC */ GpAnimRec* field_4EC; // record the frame handler last saw on slot 1
    /* 0x4F0 */ u16        field_4F0; // display state, parked at 2 by func_actor_310100_80162CDC
    /* 0x4F2 */ byte       pad_4F2[0x14];
    /* 0x506 */ u16        field_506; // passed down as the model task's spawnArg1
    /* 0x508 */ u16        field_508; // display id (0x6C / 0x6D), 0x6C selects the step-sound table
    /* 0x50A */ u16        field_50A; // next step-sound index into D_actor_310100_801798A8, capped at 2
} Actor310100Work;
STATIC_ASSERT_SIZEOF(Actor310100Work, 0x50C);

/// Step sounds the model runs through while it is on the 0x6C display id:
/// `field_50A` indexes the first three.
extern s32 D_actor_310100_801798A8[];

/// Scratch the message-0x6C state handler stages its vectors in. The state is
/// dispatched through by value, so the spawn tick's `vec` and the steady tick's
/// `rot` never overlap and the two share one stack slot.
typedef union Actor310100Vec {
    /* 0x0 */ VECTOR  vec; // model part-1 translation, handed to func_800D7A9C
    /* 0x0 */ SVECTOR rot; // floor-quad yaw, handed to Gp_DrawFloorQuad
} Actor310100Vec;

/// Model frame handler: queues the step sound for the animation record slot 1
/// has just entered — from `D_actor_310100_801798A8` while the model is on the
/// 0x6C display id, from the fixed 0x51050006 / 0x51050007 pair otherwise — then
/// ticks slots 1..0x12 and returns slot 1's `field_10` bit 0.
s32 func_actor_310100_80161E24(Task* task);

/// State handler for the display model spawned by `func_actor_310100_80162C64`:
/// the spawn tick seeds the tracker from the model's part-1 coordinate frame and
/// steps to state 1, and every later tick draws the floor quad until the display
/// state goes non-zero.
void func_actor_310100_801631B0(Task* task);

/// Second state handler of the display model spawned from
/// `D_actor_310100_801798FC` (descriptor arg 0x80168C00): the spawn tick hands
/// the model to `func_actor_310100_80162414` with display id 0x6C and steps to
/// state 1, and every later tick draws the floor quad at the model's part-1
/// frame, runs `func_actor_310100_80161F80` while the display state is 1 and
/// hands that frame's translation to `func_800D7A9C`. Display state 2, the
/// freeze parked by `func_actor_310100_80162CDC`, returns before either.
void func_actor_310100_80162F88(Task* task);

/// Second state handler of the display model spawned from
/// `D_actor_310100_80179920` (descriptor arg 0x801730B0): the spawn tick hands
/// the model to `func_actor_310100_80162414` with display id 0x6D and steps to
/// state 1, and every later tick draws the floor quad at the model's part-1
/// frame, runs `func_actor_310100_80161F80` while the display state is 1 and
/// hands that frame's translation to `func_800D7A9C`. Display state 2, the
/// freeze parked by `func_actor_310100_80162CDC`, returns before either.
void func_actor_310100_8016309C(Task* task);

/// The other display-model state handler (message 0x6D): the spawn tick seeds
/// the tracker from the model's part-1 coordinate frame and steps to state 1,
/// and every later tick draws the floor quad while the display state is still
/// below 2.
void func_actor_310100_801632B0(Task* task);

/// Message 0x7D5 handler: kills the display task hanging off the work block,
/// records the payload's `pos.vy` in the work block and spawns a fresh display
/// task from `D_actor_310100_801798E4`. The display task is handed `arg2` as its
/// `spawnArg1` and this task as its parent (`spawnArg2`); it spawns the model
/// task in turn, handing it `field_506` as its `spawnArg1`.
void func_actor_310100_80162C64(Task* task, s32 msgId, s32 arg2, Actor310100Placement* placement);

/// Message 0x7D7 handler: parks the display task's work block at state 2 and
/// returns when handed mode 3, otherwise tears the display task down and spawns
/// a fresh one from `D_actor_310100_801798F0`.
void func_actor_310100_80162CDC(Task* task, s32 msgId, s32 arg2);

/// Message 0x7D4 handler: drops the payload's translation into the display
/// task's root coordinate frame, yaws that frame to the payload's `rot.vy` and
/// marks it dirty.
void func_actor_310100_80162EC8(Task* task, s32 msgId, Actor310100Placement* placement);

/// Teardown handler: kills the display task hanging off the work block and
/// parks this task in state 3.
void func_actor_310100_80162F34(Task* task);

#endif
