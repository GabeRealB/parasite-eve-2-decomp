#ifndef ACTOR_101100_H
#define ACTOR_101100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

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
