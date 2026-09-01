#ifndef ACTOR_444000_H
#define ACTOR_444000_H

#include "common.h"

/// Per-actor work block for the `actor_444000` overlay, reached through the
/// `Task::idMap` slot (0x1C) rather than being a `TaskIdMap` here.
///
/// Only the leading state word is known so far: `func_actor_444000_80143D7C`
/// reads it with `lhu` and range-checks it, and the 0x7D9 message handler
/// `func_actor_444000_80143F38` clears it. The block is much larger than this
/// - `func_actor_444000_80143E68` touches fields up to 0xF1C - so extend the
/// struct as those functions are matched.
typedef struct Actor444000Work {
    /* 0x0 */ s16 field_0; // state index
} Actor444000Work;

typedef struct Actor444000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor444000Work* field_1C;
} Actor444000;

s32 func_actor_444000_80143F38(Actor444000* arg0);

#endif
