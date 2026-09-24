#ifndef ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
#define ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// 0xC-byte scratch block the hall's dust-mote task takes from
/// `G_SCRATCH_HEAD`. `vec` is the mote's world position copied out of the
/// task's `GsCOORDINATE2` (`workm.t`) and projected with a single `RTPS`
/// through `GsWSMATRIX`; `otz` is the `gte_stszotz` depth the resulting `TILE_1`
/// is linked into the OT at. Depths below 0x11 are dropped rather than drawn,
/// so a mote that ends up in front of the near plane costs nothing.
typedef struct _AeehMoteScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ SVECTOR vec;
} AeehMoteScratch;
STATIC_ASSERT_SIZEOF(AeehMoteScratch, 0xC);

extern const VECTOR D_acropolis_east_elevator_hall_8017D5C4;

void func_acropolis_east_elevator_hall_8017D5F0(Task* task);
void func_acropolis_east_elevator_hall_8017D7A4(Task* task);

#endif // ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
