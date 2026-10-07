#ifndef INCLUDE_ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H
#define INCLUDE_ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b2_south_maintenance_walkway_801837AC[22];

// shelter_b2_south_maintenance_walkway
extern u8* D_shelter_b2_south_maintenance_walkway_8018263C[];

extern ViewCount D_shelter_b2_south_maintenance_walkway_80182640[];

extern DirectionWarpEntry D_shelter_b2_south_maintenance_walkway_80182644[];

extern WorldCollisionGrid D_shelter_b2_south_maintenance_walkway_801829E8;

extern ViewCamera D_shelter_b2_south_maintenance_walkway_80182A0C[];

extern SpriteView D_shelter_b2_south_maintenance_walkway_80183018[];

extern WorldCoordRoomLights D_shelter_b2_south_maintenance_walkway_80183294;

extern WorldCollisionTrigger D_shelter_b2_south_maintenance_walkway_801832AC[];

extern WorldCollisionTrigger D_shelter_b2_south_maintenance_walkway_80183474[];

extern WorldCollisionOccluder D_shelter_b2_south_maintenance_walkway_8018385C[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_south_maintenance_walkway_801838B4[];

/// Receives room messages and dispatches the walkway's room-task state.
///
/// Requires state 0 (install handlers), 1 (idle) or 2 (release the task).
/// The room overlay must remain loaded while its registered task is live.
void shelterB2SouthMaintenanceWalkwayRoomTask(Task* task);

/// Draws the walkway's grey light glows and its view-4 red disc.
///
/// State 0 selects the room's effect task IDs, then advances to state 1.
/// Each tick draws only the anchors selected by mapped views 2..5; other
/// views draw nothing. Radius scale is 512 and angles use 4096 units per turn.
/// This task does not consult room-effect pause or cancellation control.
void shelterB2SouthMaintenanceWalkwayDrawGlowsTask(Task* task);

/// Runs the room's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and an owned, zero-initialized, counted
/// `EffectWork` in `spawnArg2.pointer`, as supplied by `effectSpawn`.
/// `spawnArg1.value` starts as a positive charge duration in active ticks and
/// is consumed as a countdown. Nonzero room-effect control pauses the task;
/// control 4 or above cancels it. Completion or cancellation releases the work.
void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task);

/// Records and draws blue twin trails behind two parent-relative endpoints.
///
/// Requires a coordinate body and an owned, zero-initialized, counted
/// `EffectWork` in `spawnArg2.pointer` whose `parent` stays live. State 0
/// allocates two eight-frame world-space histories in `task->work`; allocation
/// failure retries on the next active tick. `spawnArg1.value` is a recording
/// end age in 2..32767 active ticks, or 0 for no age limit. Control below 2
/// advances the trails; higher control leaves them untouched. Completion
/// releases the effect work and both histories.
void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_b2_south_maintenance_walkway_8017FCE8(Task* task);

void func_shelter_b2_south_maintenance_walkway_80180930(Task* arg0);

/// Runs an animated spark on a fixed flight step toward an initial target.
///
/// Requires a coordinate body, an owned, zero-initialized, counted `EffectWork`
/// in `spawnArg2.pointer`, and a borrowed target `GfxCoord` in `spawnArg1.pointer`.
/// Both coordinates need composed world matrices on the first active tick; the target is read
/// only then. The step is 204/4096 of the initial displacement in parent axes.
/// Age 20 releases the work. Nonzero control pauses; control 4 or above cancels.
void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the room's expanding orange disc, layered glow and fading ring.
///
/// Requires a coordinate body and an owned, zero-initialized, counted
/// `EffectWork` in `spawnArg2.pointer`, as supplied by `effectSpawn`.
/// Active ticks expand the glow and fade the ring, then fade the central disc
/// until the work is released. Nonzero room-effect control pauses the task;
/// control 4 or above cancels it and releases the work.
void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H
