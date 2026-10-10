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

// dryfield_water_tank
extern WorldCollisionRoomResources D_dryfield_water_tank_801868E0[];

/// Room 1's logical-view map, mapping views 1..10 to the same sprite/camera indices.
///
/// Indexed by room minus one; the single entry borrows `gViewIdentityMap`.
/// The map overlay borrows this directory while the water-tank overlay is loaded.
extern u8* gDryfieldWaterTankRoomViewMaps[1];

extern ViewCount D_dryfield_water_tank_801868F4[];

/// Room 1's authored light collection and ten per-view ambient minima.
///
/// Indexed by room minus one. The collection's 21 point lights are writable
/// during coordinate composition and shading; all borrowed room data must
/// remain loaded while lighting uses it.
extern WorldCoordRoomLighting gDryfieldWaterTankRoomLighting[1];

extern DirectionWarpEntry D_dryfield_water_tank_80186900[];

extern ViewCamera D_dryfield_water_tank_80186EE0[];

extern SpriteView D_dryfield_water_tank_80187F80[];

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
