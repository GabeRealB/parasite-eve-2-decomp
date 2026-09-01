#ifndef ACTOR_113100_H
#define ACTOR_113100_H

#include "common.h"

#include "gameplay/3A34.h"

/// Work block of the `actor_113100` enemy task. `func_actor_113100_80131E58`
/// (state 0) `Mem_Calloc`s 0x540 bytes and parks the pointer in the task's
/// `Task::idMap` slot -- that slot is not a `TaskIdMap` here. The same
/// function builds the display node at +0x4B8: it points `GpObj::field_C` at
/// the record that follows the node (block + 0x4D8), sets `field_18` to
/// 0x30000 and `field_1C` to 0x100, zeroes the position triple and sets
/// `flags` to 1. The exit callback `func_actor_113100_80132EF0` hands that
/// node back to `Gp_UnlinkObj`.
typedef struct Actor113100Work {
    /* 0x000 */ byte  pad_0[0x4B8];
    /* 0x4B8 */ GpObj obj;
    /* 0x4D8 */ byte  pad_4D8[0x68];
} Actor113100Work;
STATIC_ASSERT_SIZEOF(Actor113100Work, 0x540);

#endif
