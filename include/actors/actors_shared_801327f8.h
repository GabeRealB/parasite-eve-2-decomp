#ifndef ACTORS_SHARED_801327F8_H
#define ACTORS_SHARED_801327F8_H

#include "common.h"

#include "main/task.h"

/// Work block this dispatcher indexes. `turnCount` at 0x4FA is the same
/// counter `ActorsShared80132860` / `ActorsShared80132920` bump; the task's
/// `idMap` slot (0x1C) points at this block, not a `TaskIdMap`.
typedef struct ActorsShared801327f8Work {
    /* 0x000 */ byte pad_0[0x4FA];
    /* 0x4FA */ u16  turnCount;
} ActorsShared801327f8Work;

/// The four handlers this dispatcher runs. Every carrier holds its own table
/// at its own address, named there by the family's symbol maps, so the shared
/// object owns no data.
extern TaskFuncTable4 ActorsShared801327f8Table;

/// Copies the carrier's four-handler table onto the stack and runs the entry
/// `turnCount` selects.
///
/// Shared verbatim by `actor_120400`, `actor_135600`, `actor_335800` and
/// `actor_350700`.
void ActorsShared801327f8(Task* task);

#endif
