#ifndef ACTOR_511000_H
#define ACTOR_511000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block of the enemy task, reached by its model-attach children through
/// the parent task's `Task::work`. The spawn handler
/// `func_actor_511000_80133958` allocates it (`memCalloc(0x488, 0)`), hands
/// `anim` / `slots` / `field_30C` to `func_800B3F84`, and points its own model
/// at the two matrices; the three children it spawns do the same.
typedef struct Actor511000ParentWork {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[1];
    /* 0x03C */ byte       pad_3C[0x2D0];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ MATRIX     field_43C; ///< colour matrix, handed to TmdObject::colorMtx
    /* 0x45C */ MATRIX     field_45C; ///< light matrix, handed to TmdObject::lightMtx
    /* 0x47C */ s32        field_47C; ///< cleared by the spawn handler
    /* 0x480 */ s16        field_480; ///< frame counter; fades both matrices every third tick in state 3+
    /* 0x482 */ byte       pad_482[6];
} Actor511000ParentWork;
STATIC_ASSERT_SIZEOF(Actor511000ParentWork, 0x488);

#endif // ACTOR_511000_H
