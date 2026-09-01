#ifndef ACTOR_342400_H
#define ACTOR_342400_H

#include "common.h"

/// Per-actor state block for the `actor_342400` overlay's main enemy.
///
/// `func_actor_342400_80163C58` and `func_actor_342400_80163E70` both allocate
/// it with `Mem_Calloc(0x454, 0)` and store it in the `Task::idMap` slot
/// (0x1C), so the size below is the allocation, not a guess: this actor reuses
/// that pointer field for its own work block and it is *not* a `TaskIdMap`
/// here. Reach it with `(Actor342400Work*)task->idMap`.
///
/// The layout mirrors the sibling `actor_341700` overlay, whose work block is
/// the same size and drives the same state machine: `field_420` / `field_422`
/// are the state and sub-state indices the handler table walks.
typedef struct Actor342400Work {
    /* 0x000 */ byte pad_0[0x420];
    /* 0x420 */ u16  field_420; // state index
    /* 0x422 */ u16  field_422; // sub-state index
    /* 0x424 */ byte pad_424[0x30];
} Actor342400Work;
STATIC_ASSERT_SIZEOF(Actor342400Work, 0x454);

#endif
