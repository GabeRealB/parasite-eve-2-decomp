#ifndef ACTORS_SHARED_80133EAC_H
#define ACTORS_SHARED_80133EAC_H

#include "common.h"

#include "main/task.h"

/// Applies `arg2` to `task->extra`: bit 0 selects `TmdObject.flags` 0
/// (shown) vs 0x80 (hidden); bit 1 ORs in 0x4. `arg1` is unused; it exists
/// because the callers pass three arguments -- the same shape as
/// `ActorsShared801326b4`, which applies the same bits to a published task
/// rather than the argument.
s32 ActorsShared80133eac(Task* task, s32 arg1, s32 arg2);

#endif
