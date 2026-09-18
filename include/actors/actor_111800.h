#ifndef ACTOR_111800_H
#define ACTOR_111800_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block `func_actor_111800_80132390` allocates with `Mem_Calloc(0x498)`
/// and parks in `Task::work` (0x1C). The prefix is the shared actor anim
/// layout: a `GpAnimCtx` and the nineteen `GpAnimSlot`s `func_800B3F84` seeds
/// from the animation bank and the frame handler ticks. `field_43C` /
/// `field_45C` are the light and colour matrices handed to the model
/// `TmdObject`.
typedef struct Actor111800Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ MATRIX     field_43C;
    /* 0x45C */ MATRIX     field_45C;
    /* 0x47C */ void*      field_47C; // Game_GetPtrSlot(3)
    /* 0x480 */ MATRIX*    field_480; // D_80073B8C, the view matrix
    /* 0x484 */ byte       pad_484[0xE];
    /* 0x492 */ s16        field_492;
    /* 0x494 */ s16        field_494;
} Actor111800Work;
STATIC_ASSERT_SIZEOF(Actor111800Work, 0x498);

/// Animation bank `func_800B3F84` builds the work block's clip context from;
/// the actor hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_111800_8013A448[];

/// View matrix every actor walks its model against. Declared as a one-element
/// aggregate on purpose: an array element access marks the load's MEM
/// `in_struct`, which is what keeps `true_dependence` (`sched.c:846`) from
/// dropping the dependence between this load and the in-struct store to
/// `Actor111800Work::field_480` that precedes it -- as a bare `extern MATRIX*`
/// the load is a non-struct MEM at a `lo_sum` address, the suppression clause
/// fires, sched1 hoists the load above the store and local-alloc can no longer
/// reuse `$v0` after the `sw $v0, 0x47C` store.
extern MATRIX* D_80073B8C[1];

void func_actor_111800_80132390(Task* task);

#endif
