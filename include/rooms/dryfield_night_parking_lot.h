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

/// Saved-area update applied when the parking-lot lobby-unlock event starts.
///
/// In every save mode, sets the night garage's placement layout to 2, removes
/// its saved enemy poses and clears its map mark. The second record ends the
/// synchronous scan. Both parking-lot builds borrow this night-owned list;
/// the day build imports its fixed address beyond the daytime package.
extern AreaApplyRec gDryfieldNightParkingLotLobbyUnlockAreaUpdates[2];

/// Dryfield night parking lot placement/resource directory indexed by `GameLocationKey.variant` 0..21.
///
/// Slots 2, 3, 4, 7, 21 publish paired tables; all other slots are absent (NULL, NULL).
/// This placement selector is independent of the room and logical-view selectors.
/// Placements belong to the stage map; resource recipes belong to this room.
/// Keep both overlays and the actors supplying task descriptors loaded during
/// loading, spawning and model-flag updates. Lookups require an index within
/// this array; there is no terminator or bounds check.
extern AreaVariant gDryfieldNightParkingLotAreaVariants[22];

/// Lighting for logical rooms 1 and 2 of the night parking lot.
///
/// Indexed by room - 1. Both entries borrow the same mutable point-light
/// collection and use the default ambient minima. The room overlay must stay
/// loaded while either descriptor or its lights are in use.
extern WorldCoordRoomLighting gDryfieldNightParkingLotRoomLighting[2];

/// Collision resources for logical rooms 1 and 2, indexed by room - 1.
///
/// Both descriptors borrow the same mutable grid, view boundaries, action
/// triggers and sight occluders. Keep the room overlay loaded while the grid
/// is active or any trigger/occluder remains linked.
extern WorldCollisionRoomResources gDryfieldNightParkingLotRoomCollisionResources[2];

/// Logical-view maps for rooms 1 and 2 of the night parking lot.
///
/// Indexed by room - 1, then logical view - 1 for views 1..7. Room 1 borrows
/// `gViewIdentityMap`; room 2 selects mapped camera/image/sprite indices
/// 1, 8, 3, 4, 9, 10, 11. These maps have no terminator and require both
/// gameplay and the room overlay to remain loaded.
extern u8* gDryfieldNightParkingLotViewMaps[2];

/// Seven logical views in each parking-lot room, indexed by room - 1.
///
/// Counts limit logical-view searches, independently of the eleven mapped
/// camera/sprite slots. The borrowed two-entry table has no terminator.
extern ViewCount gDryfieldNightParkingLotViewCounts[2];

/// Five parking-lot arrival/departure endpoints, indexed by warp ID - 1.
///
/// Slots 1..4 serve the main street, lobby, toilet and saloon doors; slot 5
/// serves the scripted motel-balcony connection. Placements use game units
/// and 4096 yaw units per turn. Entries supply logical initial views and
/// stage-3 sound requests; slot 5 has no sound/map cue and selects scripted
/// player mode. Borrowed from the loaded overlay, with no terminator.
extern DirectionWarpEntry gDryfieldNightParkingLotWarpEntries[5];

/// Eleven parking-lot cameras, indexed by mapped view - 1.
///
/// Room 1 selects slots 1..7; room 2 selects 1, 8, 3, 4, 9, 10, 11.
/// Slots 8..11 initially duplicate 2, 5, 6, 7 in separate writable storage.
/// Transforms use world-coordinate translation and Q12 rotation; perspective
/// distances are pixels. Keep this unterminated array loaded until application.
extern ViewCamera gDryfieldNightParkingLotViewCameras[11];

/// Eleven parking-lot sprite views, indexed by mapped view - 1.
///
/// Room 1 selects slots 1..7; room 2 selects 1, 8, 3, 4, 9, 10, 11.
/// Slots 1, 7 and 11 have no source sprites. All views retain decoded
/// background strips; slots 2 and 8 also supply clipping-area lists.
/// Descriptors borrow mutable batch/source/clip arrays from this room overlay;
/// keep them loaded while preparing or drawing sprites. There is no terminator
/// in this directory; each batch and clip list has its own sentinel.
extern SpriteView gDryfieldNightParkingLotSpriteViews[11];

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
