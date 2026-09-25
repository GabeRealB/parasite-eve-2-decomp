#ifndef ROOMS_DRYFIELD_WATER_TANK_H
#define ROOMS_DRYFIELD_WATER_TANK_H

#include "common.h"

/// Toggle the room's cutscene-“watched” state over two of the area's sprite
/// commands: `arg0 != 0` hides the first and shows the second by setting and
/// clearing their `GpSprtCmd::field_4`, `arg0 == 0` does the opposite. No-op unless `GameSession.at4.loc.stage` is 2, i.e. only for the stage
/// whose sprite table has a record for the current room.
/// `func_dryfield_water_tank_8017DB48` passes the game-flag `0x55` nibble
/// through it, one way per value.
void func_dryfield_water_tank_8017EFF4(s32 arg0);

#endif // ROOMS_DRYFIELD_WATER_TANK_H
