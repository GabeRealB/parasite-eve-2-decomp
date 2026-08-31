#ifndef ACTOR_101900_H
#define ACTOR_101900_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// Private work block of the actor 01900 task, hanging off `Task::idMap`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` list nodes unlinked by the
/// destroy callback and the two child tasks it kills. `Actor01900_Fn0A764`
/// masks `field_A08.flags` and `field_B48.flags`, which is what fixes those
/// two offsets as `GpObj` rather than opaque padding.
typedef struct Actor01900Work {
    /* 0x000 */ byte  pad_0[0x8C8];
    /* 0x8C8 */ GpObj field_8C8;
    /* 0x8E8 */ byte  pad_8E8[0x120];
    /* 0xA08 */ GpObj field_A08;
    /* 0xA28 */ byte  pad_A28[0x120];
    /* 0xB48 */ GpObj field_B48;
    /* 0xB68 */ byte  pad_B68[0xD0];
    /* 0xC38 */ Task* field_C38;
    /* 0xC3C */ Task* field_C3C;
} Actor01900Work;

#endif
