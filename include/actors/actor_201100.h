#ifndef ACTOR_201100_H
#define ACTOR_201100_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// Record at `Actor201100Work::model` whose top two bits of `flags` the fade
/// state clears.
typedef struct Actor201100Model {
    /* 0x00 */ byte pad_0[0x1E];
    /* 0x1E */ u16  flags;
} Actor201100Model;
STATIC_ASSERT_SIZEOF(Actor201100Model, 0x20);

/// Work block parked in `Task::work`.
typedef struct Actor201100Work {
    /* 0x00 */ byte             pad_0[0x8];
    /* 0x08 */ Actor201100Model model;
    /* 0x28 */ byte             pad_28[0x18];
    /* 0x40 */ GpRec18          rec;
} Actor201100Work;

/// `Task::extra` of the actor: `coord` is where the spark effects spawn.
typedef struct Actor201100Extra {
    /* 0x00 */ byte           pad_0[0x8];
    /* 0x08 */ GsCOORDINATE2* coord;
} Actor201100Extra;

#endif // ACTOR_201100_H
