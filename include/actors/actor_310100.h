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
} Actor310100Work;

/// Message 0x7D4 handler: drops the payload's translation into the display
/// task's root coordinate frame, yaws that frame to the payload's `rot.vy` and
/// marks it dirty.
void func_actor_310100_80162EC8(Task* task, s32 msgId, Actor310100Placement* placement);

#endif
