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

/// 8-byte fade block `func_actor_121300_8013400C` and
/// `func_actor_121300_801326EC` each allocate with `Mem_Malloc(8, 0)` and park
/// in `Task::idMap` -- a second, smaller idMap block in this overlay, distinct
/// from `Actor121300Work`.
///
/// The three halfwords are the RGB channels `Fade_DrawOverlay` draws.  The
/// task seeded by `func_actor_121300_8013400C` seeds all three to 0 and raises
/// them by `Task::spawnArg1` every frame, then once the red channel has reached
/// 0x100 it blanks the display and kills itself; the blue channel is advanced
/// but never read back.  The task seeded by `func_actor_121300_801326EC` is the
/// mirror image: it seeds all three to 0xFF, unblanks the display one state
/// before it starts drawing, and lowers them by `Task::spawnArg1` per frame
/// until the red channel goes negative, reading all three channels back.
typedef struct Actor121300FadeWork {
    /* 0x0 */ u8  pad_0[0x2];
    /* 0x2 */ s16 r;
    /* 0x4 */ s16 g;
    /* 0x6 */ s16 b;
} Actor121300FadeWork;
STATIC_ASSERT_SIZEOF(Actor121300FadeWork, 0x8);

#endif
