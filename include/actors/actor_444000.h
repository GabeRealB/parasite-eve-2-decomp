#ifndef ACTOR_444000_H
#define ACTOR_444000_H

#include "common.h"

/// Per-actor work block for the `actor_444000` overlay, reached through the
/// `Task::idMap` slot (0x1C) rather than being a `TaskIdMap` here.
///
/// `func_actor_444000_8013AFF8` allocates it with `Mem_Calloc(0xF24, 0)` and
/// stores the result in that slot, so the size below is the allocation rather
/// than a guess. Only two fields are known so far: the leading state word,
/// which `func_actor_444000_80143D7C` reads with `lhu` and range-checks and
/// the 0x7D9 message handler `func_actor_444000_80143F38` clears, and the
/// byte at 0xEAC written by `func_actor_444000_80143490`. Fill in the padding
/// as the remaining functions are matched.
typedef struct Actor444000Work {
    /* 0x000 */ s16  field_0; // state index
    /* 0x002 */ byte pad_2[0xEAA];
    /* 0xEAC */ s8   field_EAC;
    /* 0xEAD */ byte pad_EAD[0x77];
} Actor444000Work;
STATIC_ASSERT_SIZEOF(Actor444000Work, 0xF24);

/// Body/collision object the actor task carries at +0x20 (the `Task::spawnArg2`
/// slot). Only the halfword at 0x40 is known so far: it is the remaining HP,
/// tested for `> 0` by `func_actor_444000_80143D68` and topped back up by
/// `func_actor_444000_80143E68`.
typedef struct Actor444000Obj {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ s16  field_40; // remaining HP
} Actor444000Obj;

typedef struct Actor444000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor444000Work* field_1C;
    /* 0x20 */ Actor444000Obj*  field_20;
} Actor444000;

s32 func_actor_444000_80143D68(Actor444000* arg0);
s32 func_actor_444000_80143F38(Actor444000* arg0);

#endif
