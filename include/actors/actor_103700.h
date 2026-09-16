#ifndef ACTOR_103700_H
#define ACTOR_103700_H

#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Per-actor work block, reached as `(Actor103700Work*)task->idMap`.
///
/// Like actor 421600 this overlay keeps its own state in the `Task::idMap` slot
/// instead of a `TaskIdMap`, so that pointer field is *not* a `TaskIdMap` here.
/// Only the halfword `func_actor_103700_801350DC` cycles is named so far: it
/// counts up and wraps to 0 once it passes the caller's period, and the wrapped
/// value indexes the bob table below at `arg1 * 15 + field_25E`. The sibling
/// `func_actor_103700_801347E0` drives the same counter with the same table.
typedef struct Actor103700Work {
    /* 0x000 */ byte pad_0[0x25E];
    /* 0x25E */ u16  field_25E;
} Actor103700Work;

/// Halfword bob table, one row of 15 per `arg1`: the row runs
/// 0, 10, 19, 24, 25, 22, 15, 5, -5, -15, -22, -25 before returning to 0.
/// Every use reads it as a signed halfword through `lh` and adds it to a
/// coordinate's Y translation, so it is the amplitude of an idle bob.
extern s16 D_actor_103700_80139DB8[];

#endif // ACTOR_103700_H
