#ifndef ACTOR_310100_H
#define ACTOR_310100_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actors_shared_8013411c.h"
#include "main/task.h"

/// Payload of message 0x7D4: the same world translation + Euler rotation record
/// the shared placement helpers consume, of which only the yaw (`rot.vy`) is
/// used here.
typedef ActorShared8013411cPlacement Actor310100Placement;

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. Only the prefix this body reaches is
/// described; the block continues past it (the state handlers use 0x4F0…).
typedef struct Actor310100Work {
    /* 0x000 */ byte  pad_0[0x4E4];
    /* 0x4E4 */ Task* field_4E4; // display task, killed and cleared by func_actor_310100_80162F34
    /* 0x4E8 */ byte  pad_4E8[0x8];
    /* 0x4F0 */ u16   field_4F0; // display state, parked at 2 by func_actor_310100_80162CDC
    /* 0x4F2 */ byte  pad_4F2[0x14];
    /* 0x506 */ u16   field_506; // passed down as the model task's spawnArg1
} Actor310100Work;

/// Scratch the message-0x6C state handler stages its vectors in. The state is
/// dispatched through by value, so the spawn tick's `vec` and the steady tick's
/// `rot` never overlap and the two share one stack slot.
typedef union Actor310100Vec {
    /* 0x0 */ VECTOR  vec; // model part-1 translation, handed to func_800D7A9C
    /* 0x0 */ SVECTOR rot; // floor-quad yaw, handed to Gp_DrawFloorQuad
} Actor310100Vec;

/// State handler for the display model spawned by `func_actor_310100_80162C64`:
/// the spawn tick seeds the tracker from the model's part-1 coordinate frame and
/// steps to state 1, and every later tick draws the floor quad until the display
/// state goes non-zero.
void func_actor_310100_801631B0(Task* task);

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
