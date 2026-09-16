#ifndef ACTOR_443500_H
#define ACTOR_443500_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Position context the spawn handler reads through `Task::spawnArg2` (0x20):
/// the halfword at 0x8 packs the area position, shifted down by 12 into the
/// index `Gp_GetNestedAreaRec`'s table is walked with.
typedef struct Actor443500Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
} Actor443500Ctx;

/// Work block `func_actor_443500_80132078` `Mem_Calloc`s (0x4C4) and parks in
/// the task's `Task::idMap` slot (0x1C) -- that slot is not a `TaskIdMap`
/// here, just as with `Actor335800Work` and the other sharers of
/// `ActorsShared80132f24`. The spawn handler seeds the two `sb` bytes at
/// 0x475/0x476 and the word at 0x4BC to -1 and copies the parent TmdObject's
/// flags halfword to 0x4C0; the light/colour matrix pair at 0x478/0x498 is the
/// one `ActorsShared80132f24` republishes onto the model.
///
/// The size is the allocation; the fields below are the ones this overlay's
/// decompiled bodies touch.
typedef struct Actor443500Work {
    /* 0x000 */ byte   pad_0[0x475];
    /* 0x475 */ s8     field_475;
    /* 0x476 */ s8     field_476;
    /* 0x477 */ byte   pad_477[0x1];
    /* 0x478 */ MATRIX light;
    /* 0x498 */ MATRIX color;
    /* 0x4B8 */ byte   pad_4B8[0x4];
    /* 0x4BC */ s32    field_4BC;
    /* 0x4C0 */ s32    field_4C0;
} Actor443500Work;
STATIC_ASSERT_SIZEOF(Actor443500Work, 0x4C4);

#endif // ACTOR_443500_H
