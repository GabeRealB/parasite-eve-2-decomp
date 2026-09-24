#ifndef ROOMS_DRYFIELD_WATER_TANK_H
#define ROOMS_DRYFIELD_WATER_TANK_H

#include "common.h"

/// Work block of the room's fade-out task `func_dryfield_water_tank_8017E3C4`, an
/// 8-byte `Mem_Malloc(8, 0)` parked in `Task::work` (0x1C) -- that slot is not
/// a `TaskIdMap` here. The fade-up task `func_dryfield_water_tank_8017E220`
/// keeps the same ramp: all three channels step together and the tile takes
/// its blue from `r`, so `b` is only ever stepped.
typedef struct DwtFadeWork {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ s16  r;
    /* 0x4 */ s16  g;
    /* 0x6 */ s16  b;
} DwtFadeWork;
STATIC_ASSERT_SIZEOF(DwtFadeWork, 0x8);

/// Toggle the room's cutscene-“watched” state over the view's two per-view
/// objects: `arg0 != 0` sets the first object's `field_1C` and clears the
/// second's `field_C` (so the view's sprites draw), `arg0 == 0` does the
/// opposite. No-op unless `GameSession.at4.loc.stage` is 2, i.e. only for the stage
/// whose sprite table has a record for the current room.
/// `func_dryfield_water_tank_8017DB48` passes the game-flag `0x55` nibble
/// through it, one way per value.
void func_dryfield_water_tank_8017EFF4(s32 arg0);

#endif // ROOMS_DRYFIELD_WATER_TANK_H
