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

/// Lighting for logical rooms 1 and 2 of the night parking lot.
///
/// Indexed by room - 1. Both entries borrow the same mutable point-light
/// collection and use the default ambient minima. The room overlay must stay
/// loaded while either descriptor or its lights are in use.
extern WorldCoordRoomLighting gDryfieldNightParkingLotRoomLighting[2];

extern WorldCollisionRoomResources D_dryfield_night_parking_lot_8017EE24[];

/// Logical-view maps for rooms 1 and 2 of the night parking lot.
///
/// Indexed by room - 1, then logical view - 1 for views 1..7. Room 1 borrows
/// `gViewIdentityMap`; room 2 selects mapped camera/image/sprite indices
/// 1, 8, 3, 4, 9, 10, 11. These maps have no terminator and require both
/// gameplay and the room overlay to remain loaded.
extern u8* gDryfieldNightParkingLotViewMaps[2];

extern ViewCount D_dryfield_night_parking_lot_8017EE54[];

extern DirectionWarpEntry D_dryfield_night_parking_lot_8017EE58[];

extern ViewCamera D_dryfield_night_parking_lot_8017FAF4[];

extern SpriteView D_dryfield_night_parking_lot_801805AC[];

/// Collision policies and footstep cues for night parking-lot surface classes 0..7.
///
/// Classes 0, 4 and 7 share a blocking surface without footstep cues; class 1
/// passes probes and ignores weapon impacts. Classes 2, 3, 5 and 6 block probes
/// and allow weapon impacts, with classes 5 and 6 sharing footstep cues.
/// Every class allows pushback. Records are borrowed from the loaded room
/// overlay and must not be retained after it unloads.
extern WorldCollisionSurfaceProperties* gDryfieldNightParkingLotSurfacePropertiesByClass[8];

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
