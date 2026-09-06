#ifndef ACTOR_121300_H
#define ACTOR_121300_H

#include "common.h"

#include "main/task.h"

/// Work block for the `actor_121300` overlay's cutscene actor.
///
/// `func_actor_121300_80133BFC` allocates it with `Mem_Malloc(0x4B0, 0)`,
/// zeroes it with `Mem_Set` and parks the pointer in the task's `Task::idMap`
/// slot (0x1C) -- that slot is not a `TaskIdMap` here, so reach the block with
/// `(Actor121300Work*)task->idMap`.  The same function publishes the task
/// itself in `D_actor_121300_8013D418` and stores the
/// `Game_GetPtrSlot(3)` task in `field_488`, which is the target of every
/// `Gp_DispatchMsg` the overlay sends.
typedef struct Actor121300Work {
    /* 0x000 */ byte  pad_0[0x480];
    /* 0x480 */ s16   field_480; // state index driven by func_actor_121300_80133854
    /* 0x482 */ byte  pad_482[0x6];
    /* 0x488 */ Task* field_488; // Game_GetPtrSlot(3) task, the Gp_DispatchMsg target
    /* 0x48C */ byte  pad_48C[0xC];
    /* 0x498 */ s16   field_498; // set by func_actor_121300_80134250
    /* 0x49A */ s16   field_49A; // cleared alongside field_498
    /* 0x49C */ byte  pad_49C[0x14];
} Actor121300Work;
STATIC_ASSERT_SIZEOF(Actor121300Work, 0x4B0);

#endif
