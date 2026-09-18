#ifndef ACTORS_SHARED_8013D268_H
#define ACTORS_SHARED_8013D268_H

#include "common.h"

#include "main/task.h"

/// The one field this handler touches in the carrier's work block, the
/// `Task::work` slot both carriers park their per-actor state in.
typedef struct ActorsShared8013d268Work {
    /* 0x0 */ s16 field_0;
} ActorsShared8013d268Work;

/// Message-table handler for id 0x7D5 -- the display opcode both carriers
/// list in their handler table (`D_actor_403000_80158CA8`,
/// `D_actor_312200_80169F5C`). `arg2` selects the mode: 0 hides the display
/// object by setting bit 0x80 of `TmdObject.field_C`, 1 clears `field_C` and
/// so shows it, 2 sets bit 0x4, and 3 clears `field_C` and then sets bit 0x4.
/// Modes 0 and 1 reinstate the object's buffers through `Tmd_AllocBuffers`,
/// and every mode except 1 restarts the work block's `field_0`. `arg1` is
/// unused; it exists because the dispatch passes three arguments.
s32 ActorsShared8013d268(Task* task, s32 arg1, s32 arg2);

#endif
