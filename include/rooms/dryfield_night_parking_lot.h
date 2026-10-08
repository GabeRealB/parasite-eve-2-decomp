#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
extern AreaApplyRec gParkingLotAreaRecs[2];

extern AreaVariant D_dryfield_night_parking_lot_80181438[22];

// dryfield_night_parking_lot
extern WorldCoordRoomLighting D_dryfield_night_parking_lot_8017EE14[];

extern WorldCollisionRoomResources D_dryfield_night_parking_lot_8017EE24[];

extern u8* D_dryfield_night_parking_lot_8017EE4C[];

extern ViewCount D_dryfield_night_parking_lot_8017EE54[];

extern DirectionWarpEntry D_dryfield_night_parking_lot_8017EE58[];

extern ViewCamera D_dryfield_night_parking_lot_8017FAF4[];

extern SpriteView D_dryfield_night_parking_lot_801805AC[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_parking_lot_8018153C[];

/// Updates the ambient-effect gate and draws the night parking lot's light glows.
///
/// Uses the mapped camera index for the gate and logical views 2..6 for the
/// flare and capsule selection. The room must be loaded, the mapped index
/// must address its gate table, and the view matrix, scratch stack and current
/// frame packet arena must be ready for glow drawing. `task` is unused.
void dryfieldNightParkingLotDrawGlowsTask(Task* task);

/// Runs the night parking lot's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers and restores active scavengers after
/// their entrance event; state 1 waits for messages; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightParkingLotRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_H
