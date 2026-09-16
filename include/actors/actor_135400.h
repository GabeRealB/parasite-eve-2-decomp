#ifndef ACTOR_135400_H
#define ACTOR_135400_H

#include "common.h"

#include "gameplay/3FB8.h"
#include "main/task.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Per-actor state block for the `actor_135400` overlay's enemy: the
/// `Mem_Calloc(0x498, 0)` result `func_actor_135400_80132B60` stores in the
/// `Task::idMap` slot (0x1C) -- so this actor reuses that pointer field for its
/// own work block and it is *not* a `TaskIdMap` here. Reach it with
/// `(Actor135400Work*)task->idMap`. (`func_actor_135400_80132064`, in the
/// `actor_135400` unit, carves a different, 0x4C8-byte block for its own spawn
/// path; this is not that type.)
///
/// `lightMtx` / `colorMtx` are the model's own flat-light matrices:
/// `func_actor_135400_80132CB0` points the `TmdObject`'s `field_1C` /
/// `field_20` at them and fills them from the three `D_actor_135400_8013F904`
/// lights.
///
/// `field_43C` is the per-slot flag `func_actor_135400_80132D24` raises once it
/// has set the actor up; `field_43D` / `field_43E` latch the `field_4` / `field_0`
/// of the `GpAnimArg` that call was handed (`-1` until then), and `params` holds
/// the `D_actor_135400_80131EA0` defaults.
typedef struct Actor135400Work {
    /* 0x000 */ byte      pad_0[0x43C];
    /* 0x43C */ s8        field_43C;
    /* 0x43D */ s8        field_43D;
    /* 0x43E */ s8        field_43E;
    /* 0x43F */ byte      pad_43F[0x1];
    /* 0x440 */ MATRIX    lightMtx; // the model's `TmdObject::field_1C`
    /* 0x460 */ MATRIX    colorMtx; // the model's `TmdObject::field_20`
    /* 0x480 */ GpAnimArg params;
    /* 0x494 */ s32       field_494;
} Actor135400Work;
STATIC_ASSERT_SIZEOF(Actor135400Work, 0x498);

#endif
