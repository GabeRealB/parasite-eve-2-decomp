#ifndef INCLUDE_ROOMS_DRYFIELD_DRIVEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_DRIVEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Driveway placement/resource choices indexed by placement variant 0..10.
///
/// Variant 1 places bats, 2 is empty and 3 supplies a Desert Chaser. Slots 0
/// and 4..10 are absent pairs. The directory borrows map-owned placements and
/// room-owned resource recipes; keep both overlays loaded during their use.
extern AreaVariant gDryfieldDrivewayAreaVariants[11];

/// Collision resources for driveway room selectors 1 and 2.
///
/// Indexed by room minus one. Both borrow the same grid, six view boundaries
/// and eleven action triggers; neither supplies sight occluders. Setup mutates
/// resource links and coordinates. Keep their storage loaded until unlinked.
extern WorldCollisionRoomResources gDryfieldDrivewayRoomCollisionResources[2];

/// Logical-view maps for driveway room selectors 1 and 2.
///
/// Indexed by room minus one, then logical view minus one (views 1..7).
/// Room 1 uses the identity map; room 2 uses the after-rope map, sending
/// logical view 4 to resource slot 7 and logical view 7 to slot 4.
/// Results are 1-based camera/image/sprite indices. Borrow while this overlay is loaded.
extern u8* gDryfieldDrivewayRoomViewMaps[2];

/// Lighting choices for driveway room selectors 1 and 2.
///
/// Indexed by room minus one. Both borrow the same twelve point lights and
/// use the default ambient minima. Records and lights stay live with this overlay.
extern WorldCoordRoomLighting gDryfieldDrivewayRoomLighting[2];

/// Seven logical views for each driveway room selector, indexed by room minus one.
///
/// Counts limit reverse searches of the corresponding view map; there is no
/// terminator. Borrow the two signed-halfword counts while this overlay is loaded.
extern ViewCount gDryfieldDrivewayRoomViewCounts[2];

/// Three driveway arrival/departure endpoints, indexed by warp ID minus one.
///
/// Slots 1, 2 and 3 serve the factory, water hole and main street respectively.
/// Positions use room game coordinates and yaw uses 4096 units per turn.
/// Slot 2 faces the transition point; the other player placements face yaw 1024.
/// The water-hole endpoint has no sounds or map marker. There is no terminator;
/// borrow these records while this overlay is loaded.
extern DirectionWarpEntry gDryfieldDrivewayWarpEntries[3];

/// Seven driveway cameras, indexed by mapped view minus one.
///
/// Slots 4 and 7 share a camera pose for the before/after-rope backgrounds.
/// Borrowed records must remain loaded until a queued camera update applies them.
extern ViewCamera gDryfieldDrivewayViewCameras[7];

/// Sprite layers and clipping lists for the seven mapped driveway views.
///
/// Indexed by mapped view minus one, independently of the logical view.
/// Slots 1 and 6 contain no source sprites; slots 4 and 7 supply the
/// before/after-rope layers. Descriptors and backing lists stay live with this overlay.
extern SpriteView gDryfieldDrivewaySpriteViews[7];

/// Driveway collision policies indexed by surface class 0..7.
///
/// Class 1 passes probes and disables weapon impacts; class 2 supplies footstep
/// sounds. Other classes block probes and allow impacts without footstep sounds.
/// All classes apply pushback. Borrow these records while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldDrivewaySurfacePropertiesByClass[8];

/// Enables the current view's ambient effects on every room-effect update.
///
/// Requires the initialized room-effect controller at `gRoomEffectState`.
/// The task argument is ignored; the callback neither advances nor ends it.
void dryfieldDrivewayEnableAmbientEffectsTask(Task* unusedTask);

/// Runs the driveway room task's initialization, idle or teardown state.
///
/// Requires a live task with state 0 initialize, 1 idle, or 2 destroy. State 0
/// installs the room messages, registers `GAME_TASK_SLOT_ROOM` and enters 1.
/// Dispatch uses a by-value copy of the three handlers; no work is allocated.
void dryfieldDrivewayRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_DRIVEWAY_H
