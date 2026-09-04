#ifndef ROOMS_ACROPOLIS_SANCTUARY_H
#define ROOMS_ACROPOLIS_SANCTUARY_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// 0xC work block of the sanctuary's cutscene task, hung off the `Task::idMap`
/// slot (0x1C) -- that slot is *not* a `TaskIdMap` here, it is the
/// `Mem_Calloc(0xC)` block `func_acropolis_sanctuary_8017DA40` allocates and
/// zeroes before publishing the owning task in
/// `D_acropolis_sanctuary_80186C90`. Reach it with `(AcsCutsceneWork*)task->idMap`.
///
/// `target` is the slot-3 task the block's messages are addressed to, captured
/// once from `Game_GetPtrSlot(3)`. `phase` is the script step the driver in
/// `func_acropolis_sanctuary_8017DA40` runs -- it only acts on phase 2, and
/// then only while `step` is still 0, bumping `step` once the scene has been
/// dispatched so it fires exactly once.
typedef struct AcsCutsceneWork {
    /* 0x0 */ Task* target;
    /* 0x4 */ s16   phase;
    /* 0x6 */ s16   step;
    /* 0x8 */ s32   field_8;
} AcsCutsceneWork;
STATIC_ASSERT_SIZEOF(AcsCutsceneWork, 0xC);

#endif
