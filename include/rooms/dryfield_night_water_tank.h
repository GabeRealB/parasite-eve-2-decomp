#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TANK_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TANK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_water_tank_80180764[13];

/// Model lighting for the night water-tank room, indexed by room minus one.
///
/// Room 1 borrows four directional lights and uses the default ambient minima.
/// The collection and writable light transforms stay live while this overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightWaterTankRoomLighting[1];

/// Collision resources for night Dryfield stage 3 area 21's only room.
///
/// Indexed by room ID minus one; the stage map borrows this one-entry array.
/// The grid, four view boundaries, eight actions and two sight occluders remain
/// writable during binding and contact dispatch. Keep the overlay loaded until
/// the grid is retired and the triggers and occluders are unlinked.
extern WorldCollisionRoomResources gDryfieldNightWaterTankCollisionRooms[1];

/// Logical-view mapping for room 1, indexed by room minus one.
///
/// The first ten bytes map logical views 1..10 to camera/image/sprite indices
/// 1..6: views 1..5 map directly, and views 6..10 share index 6. There is no
/// terminator; two retained zero bytes lie outside the logical-view range.
/// The directory and writable map belong to the loaded room overlay.
extern u8* gDryfieldNightWaterTankViewMaps[1];

/// Ten logical views searched through room 1's logical-view map.
///
/// This one-entry room array stores a signed halfword count, with no terminator.
/// It counts views 1..10, which map to six camera/image/sprite entries; it does
/// not include the map's two retained zeros. The stage map borrows the array
/// while this room overlay is loaded.
extern ViewCount gDryfieldNightWaterTankViewCounts[1];

/// Two night water-tank warp endpoints, indexed by warp ID minus one.
///
/// IDs 1/2 supply player and companion world placements and initial logical
/// views 2/4. The tower exit uses endpoint 1; endpoint 2 retains the view-4
/// placement. Yaw uses 4096 units per turn. Both entries have no sounds, map
/// flag or scripted/fade options. There is no terminator; the stage directory
/// borrows the array while this overlay is loaded.
extern DirectionWarpEntry gDryfieldNightWaterTankWarpEntries[2];

/// Room cameras indexed by mapped view minus one, for mapped views 1..6.
///
/// Rotation uses ONE (4096) for unit scale, translation is the negated camera
/// origin in world units, and projection distance is in pixels. There is no
/// terminator. Queued camera application requires the room overlay to stay loaded.
extern ViewCamera gDryfieldNightWaterTankViewCameras[6];

/// Background sprite resources indexed by mapped view minus one, for views 1..6.
///
/// Every view retains decoded image strips through its first zero-count batch.
/// Views 2..5 add depth-sorted rectangles; views 1 and 6 retain their batch-list
/// address in the unused source slot. All clipping lists are NULL. Descriptors,
/// sources and mutable batch lists remain live while this room overlay is loaded.
extern SpriteView gDryfieldNightWaterTankSpriteViews[6];

/// Collision policies and footstep cue sets indexed by surface class 0..7.
///
/// Classes 0, 2, 6 and 7 share silent blocking contact. Class 1 passes probes
/// and ignores weapon impacts; classes 3..5 block and allow impacts with their
/// own footstep cues. Every class applies pushback. Records and cue sets belong
/// to the loaded room overlay; the directory has no terminator.
extern WorldCollisionSurfaceProperties* gDryfieldNightWaterTankSurfacePropertiesByClass[8];

/// No-op callback for the night water-tank room's effect task.
///
/// Gameplay selects effect bank 6, slot 0x111 for this room. The callback draws
/// nothing and leaves the task, its work and coordinate body unchanged; it does
/// not advance or release the task. The room overlay must remain loaded while
/// the task can invoke this callback.
void dryfieldNightWaterTankNoOpEffectTask(Task* unusedTask);

/// Runs the night water tank's room initialization, timer update and teardown.
///
/// State 0 installs the room handlers and prepares the tank and visit
/// actors; state 1 holds the Ice Bag timer on variant 11; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightWaterTankRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TANK_H
