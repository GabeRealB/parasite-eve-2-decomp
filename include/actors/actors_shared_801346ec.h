#ifndef ACTORS_SHARED_801346EC_H
#define ACTORS_SHARED_801346EC_H

#include "common.h"

#include "main/task.h"

/// Hides the task's `TmdObject` (bit 0x80 of `field_C`) while `arg2` is zero,
/// and clears that bit otherwise. `arg1` is unused; it exists because the
/// callers pass three arguments -- the same shape as `ActorsShared801326b4`,
/// which owns the other bits of the same field.
void ActorsShared801346ec(Task* task, s32 arg1, s32 arg2);

#endif // ACTORS_SHARED_801346EC_H
