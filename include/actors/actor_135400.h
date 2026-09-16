#ifndef ACTOR_135400_H
#define ACTOR_135400_H

#include "common.h"
#include "main/task.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Per-actor state block for the `actor_135400` overlay's enemy, the
/// `Mem_Calloc(0x4C8, 0)` result `func_actor_135400_80132064` stores in the
/// `Task::idMap` slot (0x1C) -- so this actor reuses that pointer field for its
/// own work block and it is *not* a `TaskIdMap` here. The size below is that
/// allocation. Reach it with `(Actor135400Work*)task->idMap`.
///
/// `lightMtx` / `colorMtx` are the model's own flat-light matrices:
/// `func_actor_135400_80132CB0` points the `TmdObject`'s `field_1C` /
/// `field_20` at them and fills them from the three `D_actor_135400_8013F904`
/// lights.
typedef struct Actor135400Work {
    /* 0x000 */ byte   pad_0[0x440];
    /* 0x440 */ MATRIX lightMtx; // the model's `TmdObject::field_1C`
    /* 0x460 */ MATRIX colorMtx; // the model's `TmdObject::field_20`
    /* 0x480 */ byte   pad_480[0x48];
} Actor135400Work;
STATIC_ASSERT_SIZEOF(Actor135400Work, 0x4C8);

#endif
