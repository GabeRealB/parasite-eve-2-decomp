#ifndef ACTORS_SHARED_801326B4_H
#define ACTORS_SHARED_801326B4_H

#include "common.h"

#include "main/task.h"

/// The overlay's published task. Each carrier's spawn routine stores it
/// (`ActorsShared80131f9cSub0` on the 80131f9c family; the equivalent setup
/// on the others). Named per overlay in the family's symbol maps so one
/// shared object links into all of them.
extern Task* ActorsShared801326b4Task;

/// Applies `arg2` to that task's extra: bit 0 selects `TmdObject.field_C`
/// 0 (shown) vs 0x80 (hidden); bit 1 ORs in 0x4.
s32 ActorsShared801326b4(Task* task, s32 arg1, s32 arg2);

#endif
