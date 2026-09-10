#ifndef ACTORS_SHARED_8016945C_H
#define ACTORS_SHARED_8016945C_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Moves the task to `state` and rewinds the work block's state machine.
/// Inlined: each call reloads `idMap`, and the parameter copy is what puts
/// the task in `$a1` in `ActorsShared8016945c`.
static inline void ActorsShared_SetTaskState(Task* task, s32 state)
{
    ActorsShared80168d3cWork* work = (ActorsShared80168d3cWork*)task->idMap;

    task->state     = state;
    work->field_420 = 0;
    work->field_422 = 0;
}

/// Jumps the work block's state machine to `state`, sub-state 0.
static inline void ActorsShared_SetWorkState(Task* task, s32 state)
{
    ActorsShared80168d3cWork* work = (ActorsShared80168d3cWork*)task->idMap;

    work->field_420 = state;
    work->field_422 = 0;
}

/// Once bit 7 of `Gp_StateF0.field_1F` is set, puts the task in state 3 with
/// its work block at state 5 and returns 1; otherwise returns 0 and leaves
/// the task alone. Shared by `actor_341700` and `actor_342400`, whose work
/// blocks are the same `ActorsShared80168d3cWork`.
s16 ActorsShared8016945c(Task* arg0);

#endif
