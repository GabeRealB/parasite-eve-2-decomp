#ifndef ROOMS_ROOM_COMMON_H
#define ROOMS_ROOM_COMMON_H

#include "common.h"

#include "main/task.h"

#include <psyq/libgte.h>

/// Work object a `Gp_State1C`-pool room effect task keeps at `Task::spawnArg2`
/// (released with `Gp_ReleaseState1CMem`). `field_10` is the `SVECTOR` handed to
/// `Gp_SpawnEff`; `field_22` is the per-frame tick the task compares against its
/// lifetime; `field_24` / `field_26` are the animated colour and size ramps and
/// `field_2A` the per-frame step derived from `Task::spawnArg1`.
///
/// Shared by every room overlay that spawns effects this way
/// (`shelter_b6_nursery`, `acropolis_fire_escape`, …).
typedef struct RoomEffWork {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR field_10;
    /* 0x18 */ byte    pad_18[0xA];
    /* 0x22 */ u16     field_22;
    /* 0x24 */ u16     field_24;
    /* 0x26 */ u16     field_26;
    /* 0x28 */ byte    pad_28[2];
    /* 0x2A */ u16     field_2A;
} RoomEffWork;

/// Position + Euler rotation handed to `Room_Util18`, which copies it onto a
/// task's `TmdObject` coordinate frame (`Task::extra->field_8`): the three
/// longs become the coordinate's translation and the three shorts are fed to
/// `RotMatrixZYX`. Room overlays keep one of these per placed object in their
/// own `.data`.
typedef struct RoomPlacement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} RoomPlacement;

// =============================================================================
// Functions — shared room library (src/rooms/lib)
// =============================================================================

s32 Room_Util18(Task* task, s32 arg1, RoomPlacement* placement, s32 arg3);

#endif // ROOMS_ROOM_COMMON_H
