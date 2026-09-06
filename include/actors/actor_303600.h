#ifndef ACTOR_303600_H
#define ACTOR_303600_H

#include "common.h"

#include "main/task.h"

/// Work block for the `actor_303600` overlay's cutscene controller.
///
/// `func_actor_303600_8016216C` allocates it with `Mem_Malloc(0x10, 0)`, zeroes
/// it with `Mem_Set` and parks the pointer in the task's `Task::idMap` slot
/// (0x1C) -- that slot is not a `TaskIdMap` here, so reach the block with
/// `(Actor303600Work*)task->idMap`.  The same function publishes the task
/// itself in `D_actor_303600_8016E4C0` and stores the `Game_GetPtrSlot(3)` task
/// in `field_0`.
///
/// `command` is the request the overlay's state machine dispatches on:
/// `func_actor_303600_80161F40` switches on it through
/// `jtbl_actor_303600_80161E24` (values 0..8) and clears it again on the way
/// out.  `field_C` records the message the dispatcher last sent and `field_E`
/// is the "a message is outstanding" flag that
/// `func_actor_303600_801624B0` / `func_actor_303600_8016253C` test before
/// sending another.
typedef struct Actor303600Work {
    /* 0x0 */ Task* field_0; // Game_GetPtrSlot(3) task
    /* 0x4 */ s16   command; // state-machine request, see jtbl_actor_303600_80161E24
    /* 0x6 */ s16   field_6; // cleared alongside command
    /* 0x8 */ byte  pad_8[0x4];
    /* 0xC */ s16   field_C; // message id last dispatched
    /* 0xE */ u16   field_E; // set to 1 while a dispatched message is outstanding
} Actor303600Work;
STATIC_ASSERT_SIZEOF(Actor303600Work, 0x10);

#endif // ACTOR_303600_H
