#ifndef ROOMS_ACROPOLIS_SANCTUARY_H
#define ROOMS_ACROPOLIS_SANCTUARY_H

#include "common.h"

#include "gameplay/3CD8.h"
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
    /* 0x4 */ u16   phase;
    /* 0x6 */ u16   step;
    /* 0x8 */ s32   field_8;
} AcsCutsceneWork;
STATIC_ASSERT_SIZEOF(AcsCutsceneWork, 0xC);

/// Scratch payload `func_acropolis_sanctuary_8017DA40` builds on its own stack
/// for the slot-3 messages it sends: `rec` is the 0x14-byte record msgs 0x3E8
/// (weapon) and 0x3F4 take, `place` the position + Euler rotation msg 0x3E9
/// takes. One buffer serves both because the task only ever has one message in
/// flight, and the union is what makes the 0x18-byte frame slot the two share
/// explicit.
typedef union AcsMsgArg {
    /* 0x0 */ GpRec14       rec;
    /* 0x0 */ RoomPlacement place;
} AcsMsgArg;
STATIC_ASSERT_SIZEOF(AcsMsgArg, 0x18);

#endif
