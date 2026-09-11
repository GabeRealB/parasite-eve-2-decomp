#ifndef ACTORS_SHARED_8013A0B0_H
#define ACTORS_SHARED_8013A0B0_H

#include "common.h"

#include "main/task.h"

/// Status flags at + 0xFC of the `Task::idMap` work block, read through two
/// widths: bit 0 as a halfword, then bits 0x102 as a word. In
/// `Actor400600Work` the word sits over `slots[1].field_10`.
typedef union ActorsShared8013a0b0Flags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} ActorsShared8013a0b0Flags;
STATIC_ASSERT_SIZEOF(ActorsShared8013a0b0Flags, 0x4);

/// The part of the `actor_400600` / `actor_405800` work block this body reads.
typedef struct ActorsShared8013a0b0Work {
    /* 0x00 */ byte                      pad_0[0xFC];
    /* 0xFC */ ActorsShared8013a0b0Flags flags_FC;
} ActorsShared8013a0b0Work;

/// Returns 1 when bit 0 of `flags_FC`'s halfword or any of bits 0x102 of the
/// whole word is set, 0 otherwise. Shared by `actor_400600` and
/// `actor_405800`; `actor_400600` calls it unprototyped, sometimes without an
/// argument, so the caller's own `Task*` stays in `$a0` without a copy.
s32 ActorsShared8013a0b0(Task* arg0);

#endif
