#ifndef ACTOR_421600_H
#define ACTOR_421600_H

#include "common.h"

/// Per-actor state block for the `actor_421600` overlay's enemy.
///
/// `func_actor_421600_80134AD4` allocates it with `Mem_Calloc(0xEB0, 0)` and
/// stores it in the `Task::idMap` slot (0x1C), so the size below is the
/// allocation rather than a guess: this actor reuses that pointer field for its
/// own work block and it is *not* a `TaskIdMap` here. Reach it with
/// `(Actor421600Work*)task->idMap`.
typedef struct Actor421600Work {
    /* 0x000 */ byte pad_0[0xEAC];
    /* 0xEAC */ s16  field_EAC;
    /* 0xEAE */ byte pad_EAE[2];
} Actor421600Work;
STATIC_ASSERT_SIZEOF(Actor421600Work, 0xEB0);

#endif
