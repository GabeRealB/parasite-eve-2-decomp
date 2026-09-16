#ifndef ACTOR_101100_H
#define ACTOR_101100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Work block the spawn state allocates with `Mem_Calloc(0x58, 0)` and parks in
/// the `Task::idMap` slot (0x1C), which is not a `TaskIdMap` here. Only the
/// fields the two state handlers touch are filled in: the `GpObj` at 0x08,
/// whose `flags` carry the 0xC000 pair the spawn state ORs in and the per-frame
/// state clears, and the one-entry `GpRec18` collision table at 0x40 that the
/// object's `field_C` points at (`Gp_InitRec18Table(_, 1, 0)`).
typedef struct Actor101100Work {
    /* 0x00 */ byte    pad_0[8];
    /* 0x08 */ GpObj   obj;
    /* 0x28 */ byte    pad_28[0x18];
    /* 0x40 */ GpRec18 rec[1];
} Actor101100Work;
STATIC_ASSERT_SIZEOF(Actor101100Work, 0x58);

/// Absolute; nonzero skips the per-frame state handler entirely.
extern u8 D_801153F4;

/// One of the actor's three state handlers - spawn/setup, per-frame tick and
/// teardown. Wider than the usual two-argument `GpEnemyTaskFunc` shape: the
/// handlers also take the actor's work block (`Task::idMap`) and a 0x68-byte
/// scratchpad buffer the dispatcher allocates around the call.
typedef void (*Actor101100StateFunc)(GpEnemy* enemy, Task* task, void* work, void* scratch);

/// Fixed-size table of `Actor101100StateFunc` callbacks. Copied onto the stack
/// by `func_actor_101100_80138374` so the call uses a local jump table.
typedef struct {
    Actor101100StateFunc funcs[3];
} Actor101100StateFuncTable3;

#endif // ACTOR_101100_H
