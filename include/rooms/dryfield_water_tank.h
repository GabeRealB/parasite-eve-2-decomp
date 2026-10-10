#ifndef INCLUDE_ROOMS_DRYFIELD_WATER_TANK_H
#define INCLUDE_ROOMS_DRYFIELD_WATER_TANK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_water_tank_80188BF0[13];

/// Number of mapped camera, sprite and ambient-effect slots in the water-tank room.
enum { DRYFIELD_WATER_TANK_MAPPED_VIEW_COUNT = 10 };

/// Room 1's collision mesh, four view boundaries and nine action triggers.
///
/// Indexed by room minus one. Both mutable trigger arrays include their final
/// WORLD_COLLISION_TRIGGER_LAST record; there are no sight occluders. The map
/// overlay borrows the directory while this room is loaded. Keep its grid live
/// while active and its trigger storage live until the runtime lists are unlinked.
extern WorldCollisionRoomResources gDryfieldWaterTankRoomCollisionResources[1];

/// Room 1's logical-view map, mapping views 1..10 to the same sprite/camera indices.
///
/// Indexed by room minus one; the single entry borrows `gViewIdentityMap`.
/// The map overlay borrows this directory while the water-tank overlay is loaded.
extern u8* gDryfieldWaterTankRoomViewMaps[1];

/// Room 1's ten-view logical search count, followed by a retained zero halfword.
///
/// Count lookup uses room minus one; only room 1 has an established view map.
/// The second halfword has no established consumer or role and is not a proven
/// terminator or second room. Its stored extent is retained. The map overlay
/// borrows the signed-halfword storage while this room overlay is loaded.
extern ViewCount gDryfieldWaterTankRoomViewCounts[2];

/// Room 1's authored light collection and ten per-view ambient minima.
///
/// Indexed by room minus one. The collection's 21 point lights are writable
/// during coordinate composition and shading; all borrowed room data must
/// remain loaded while lighting uses it.
extern WorldCoordRoomLighting gDryfieldWaterTankRoomLighting[1];

/// Two silent player/companion warp endpoints, indexed by warp ID minus one.
///
/// Slots 1/2 place both actors at their authored world positions and start in
/// logical views 2/4. Yaw uses 4096 units per turn. Neither endpoint selects
/// optional warp flags or a map marker; there is no terminator. The map overlay
/// borrows the entries while this room is loaded; transitions copy whole records.
extern DirectionWarpEntry gDryfieldWaterTankWarpEntries[2];

/// Authored camera transforms and perspective distances for mapped views 1..10.
///
/// Indexed by mapped view minus one, without a terminator. Rotation uses Q12,
/// translation is the negated camera origin in world units, and screen distance
/// is in pixels. The map overlay borrows this array while the room is loaded;
/// queued camera applications require the selected record to remain live.
extern ViewCamera gDryfieldWaterTankViewCameras[DRYFIELD_WATER_TANK_MAPPED_VIEW_COUNT];

/// Background and foreground sprite resources for mapped views 1..10.
///
/// Indexed by mapped view minus one, without a terminator. Views 1, 6, 7 and 10
/// have empty source lists but retain decoded backgrounds; every draw-area list
/// is NULL. Mechanism operation toggles batches in views 3 and 8. The map borrows
/// this directory and its mutable sources/batches while the room remains loaded.
extern SpriteView gDryfieldWaterTankSpriteViews[DRYFIELD_WATER_TANK_MAPPED_VIEW_COUNT];

/// Collision and footstep policies indexed by surface class 0..7.
///
/// Classes 0, 2, 6 and 7 share a silent blocking record. Class 1 passes probes
/// and suppresses weapon impacts; classes 3..5 select their own footstep cues.
/// Every class applies pushback. The map overlay borrows this directory and
/// its records only while the water-tank overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldWaterTankSurfacePropertiesByClass[8];

/// Updates the ambient-effect gate for the current water-tank camera view.
///
/// Mapped view 9 publishes `ROOM_EFFECT_VIEW_ENABLED`; views 1..8 and 10 publish
/// `ROOM_EFFECT_VIEW_DISABLED`. Requires initialized `gRoomEffectState`, the
/// water-tank overlay and view resources to remain loaded, and a mapped index
/// in 1..10. The mapping and table lookup are unchecked. The task argument is
/// unused; the callback runs every frame without changing state or ending itself.
void dryfieldWaterTankUpdateViewEffectGateTask(Task* task);

/// Runs the water-tank room's initialization, ambience and teardown states.
///
/// State 0 installs room messages, registers the room task, spawns the room's
/// swaying tank model, starts ambience and synchronizes mechanism sprites. State 1
/// updates view-specific ambience each frame; state 2 kills the task.
/// Requires `task->state` in 0..2 and the room overlay/resources to remain loaded.
void dryfieldWaterTankRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WATER_TANK_H
