#ifndef ACTOR_210700_H
#define ACTOR_210700_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// 0x540-byte work block allocated by `func_actor_210700_80149F90`
/// (`Mem_Calloc(0x540, 0)`) and parked in that task's `Task::idMap` slot.
/// `func_actor_210700_8014A208` republishes the two matrices onto the model's
/// `TmdObject::field_1C` / `field_20` -- the light/colour matrix pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`
/// -- so this actor lights itself out of its own block.
///
/// The three fields named below are the ones that init writes before the
/// state machine runs; everything else is still padding.
typedef struct Actor210700Work {
    /* 0x000 */ byte   pad_0[0x478];
    /* 0x478 */ s32    field_478; // init'd to -1
    /* 0x47C */ s32    field_47C; // init'd to -1
    /* 0x480 */ MATRIX light;
    /* 0x4A0 */ MATRIX color;
    /* 0x4C0 */ byte   pad_4C0[0x7E];
    /* 0x53E */ s16    field_53E; // init'd to -1
} Actor210700Work;
STATIC_ASSERT_SIZEOF(Actor210700Work, 0x540);

void func_actor_210700_8014A208(Task* arg0);

#endif
