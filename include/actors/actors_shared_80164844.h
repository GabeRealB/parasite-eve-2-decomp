#ifndef ACTORS_SHARED_80164844_H
#define ACTORS_SHARED_80164844_H

#include "common.h"

#include "main/task.h"

/// The one field this handler touches in the carrier's work block, the
/// `Task::idMap` slot both carriers park their per-actor state in.
typedef struct ActorsShared80164844Work {
    /* 0x0 */ s16 field_0;
} ActorsShared80164844Work;

/// Message-table handler for id 0x7D5 -- the display opcode both carriers
/// list in their handler table (`D_actor_323000_801739D0`,
/// `D_actor_323400_801711D4`). `arg2` selects the mode: 0 hides the display
/// object by setting bit 0x80 of `TmdObject.field_C`, 1 clears `field_C` and
/// so shows it, 2 sets bit 0x4, and 3 clears `field_C` and then sets bit 0x4.
/// Modes 0 and 1 reinstate the object's buffers through `Tmd_AllocBuffers`;
/// modes 0, 2 and 3 restart the work block's `field_0` at 0, where mode 1
/// selects clip 2 instead. `arg1` is unused; it exists because the dispatch
/// passes three arguments.
s32 ActorsShared80164844(Task* task, s32 arg1, s32 arg2);

#endif
