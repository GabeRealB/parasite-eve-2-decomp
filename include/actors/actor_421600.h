#ifndef ACTOR_421600_H
#define ACTOR_421600_H

#include "common.h"

#include <psyq/libgte.h>

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

/// The overlay's pose table: 8-byte records of three halfwords at 0x0/0x2/0x4
/// plus padding, i.e. `SVECTOR`s. Indexed by the low signed halfword of the
/// caller's id. `actor_403000` keeps a table of the same shape at 0x80158CE0
/// and reaches it with a body the shared-body index groups with this one; a
/// body that reads its own overlay's data cannot be promoted, so each carrier
/// keeps a plain-C copy -- `src/actors/actor_403000/actor_403000.c` for the
/// other.
extern SVECTOR D_actor_421600_80151158[];

/// 4-byte table indexed by `(arg0 > 0) + ((arg1 < 1) << 1)`.
extern s8 D_actor_421600_801511D0[];

/// Copy the `vx`/`vy`/`vz` of record `arg1` of the pose table into `arg0`.
void func_actor_421600_8013E7F8(SVECTOR* arg0, s32 arg1);

s8 func_actor_421600_8013E830(s32 arg0, s32 arg1);

#endif
