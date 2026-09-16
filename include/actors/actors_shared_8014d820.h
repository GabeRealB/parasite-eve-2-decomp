#ifndef ACTORS_SHARED_8014D820_H
#define ACTORS_SHARED_8014D820_H

#include "common.h"

#include "main/task.h"

/// The one field this handler touches in the carrier's work block, the
/// `Task::idMap` slot each carrier parks its per-instance state in.
typedef struct ActorsShared8014d820Work {
    /* 0x0 */ s16 field_0;
} ActorsShared8014d820Work;

/// Mode handler for the actor's display object (`Task::extra`), carried by the
/// three stage slots the actor appears in. `arg2` selects the mode: 0 hides the
/// object by setting bit 0x80 of `TmdObject.field_C`, 1 clears `field_C` and so
/// shows it, 2 sets bit 0x4, and 3 clears `field_C` and then sets bit 0x4.
/// Modes 0 and 1 reinstate the object's buffers through `Tmd_AllocBuffers`, and
/// every mode restarts the work block's `field_0` - 7 from the two buffer
/// modes, 0 from the other two. `arg1` is unused; it exists because the
/// dispatch passes three arguments.
s32 ActorsShared8014d820(Task* task, s32 arg1, s32 arg2);

#endif
