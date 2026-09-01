#ifndef ACTOR_104400_H
#define ACTOR_104400_H

#include "common.h"

/// Per-actor state block for the `actor_104400` overlay's enemy.
///
/// `Actor04400_Fn00B24` and `Actor04400_Fn00D3C` both allocate it with
/// `Mem_Calloc(0x454, 0)` and store it in the `Task::idMap` slot (0x1C), so
/// the size below is the allocation, not a guess: this actor reuses that
/// pointer field for its own work block and it is *not* a `TaskIdMap` here.
/// Reach it with `(Actor104400Work*)task->idMap`.
///
/// The layout mirrors the sibling `actor_341700` / `actor_342400` overlays,
/// whose work blocks are the same size and drive the same state machine:
/// `field_420` / `field_422` are the state and sub-state indices the handler
/// table walks.
typedef struct Actor104400Work {
    /* 0x000 */ byte pad_0[0x420];
    /* 0x420 */ u16  field_420; // state index
    /* 0x422 */ u16  field_422; // sub-state index
    /* 0x424 */ byte pad_424[0x2B];
    /* 0x44F */ u8   field_44F;
    /* 0x450 */ byte pad_450[0x4];
} Actor104400Work;
STATIC_ASSERT_SIZEOF(Actor104400Work, 0x454);

#endif
