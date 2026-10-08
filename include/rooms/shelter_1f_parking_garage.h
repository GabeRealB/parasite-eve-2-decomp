#ifndef INCLUDE_ROOMS_SHELTER_1F_PARKING_GARAGE_H
#define INCLUDE_ROOMS_SHELTER_1F_PARKING_GARAGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_1f_parking_garage_801818D8[12];

// shelter_1f_parking_garage
extern WorldCollisionRoomResources D_shelter_1f_parking_garage_80180C64[];

extern WorldCoordRoomLighting D_shelter_1f_parking_garage_80180C74[];

extern u8* D_shelter_1f_parking_garage_80180C7C[];

extern ViewCount D_shelter_1f_parking_garage_80180C80[];

extern DirectionWarpEntry D_shelter_1f_parking_garage_80180C84[];

extern ViewCamera D_shelter_1f_parking_garage_8018100C[];

extern SpriteView D_shelter_1f_parking_garage_80181430[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_parking_garage_80181954[];

/// Runs the parking garage's room controller for one tick.
///
/// Requires a live bodyless task with state 0..2 and this room loaded. State 0
/// installs room messages and handles warp-1 arrival, state 1 remains idle for
/// messages, and state 2 requests teardown. Allocates no work of its own.
void shelter1fParkingGarageRoomTask(Task* task);

/// Installs the room's actor-effect IDs and draws fixed glows in mapped views 2 and 4.
///
/// Requires a live room-effect controller, composed view matrices and frame
/// packet/scratch capacity. State 0 publishes the flash, twin-trail and spark-burst
/// IDs, then becomes state 1. View 2 draws four amber capsules; view 4 draws a red
/// disc and two amber capsules. Other views draw none. Radius scales are 512 for
/// capsules and 768 for the disc, projected as scale * 64 / (camera Z / 4) pixels.
/// Each capsule's first depth is clamped to 16; its second end and the disc require
/// depth at least 17. Room effect control does not gate this task.
void shelter1fParkingGarageDrawViewGlowsTask(Task* task);

/// Runs the room's charging pink flash, peak screen tint and fading star.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. `spawnArg1.value` is a positive charge duration in callback
/// ticks, consumed as a countdown after initialization. Nonzero room effect
/// control pauses the task; values 4 and above cancel it. State 3 requests
/// release. Completion or cancellation releases the counted work and task.
void shelter1fParkingGarageRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading blue twin-trail beam.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`, a live parent coordinate, and `Task::work` initially NULL.
/// Borrows the parent for its lifetime and owns two eight-coordinate histories
/// in `Task::work`; allocation failure retries initialization. `spawnArg1.value`
/// is zero for an unlimited lifetime, or 2..32767 for a release age in active
/// ticks. Initialization uses age 1 without checking expiry. Room effect control
/// values 2 and above freeze updates and drawing. Effect teardown releases both
/// the counted work and history block.
void shelter1fParkingGarageRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the parking garage's impact flash and smoke or orange-ring spark burst.
///
/// Requires a counted coordinate-body effect with zero-aged owned `EffectWork`
/// in `spawnArg2.pointer` and state 0, as supplied by `effectSpawn`.
/// Nonzero `spawnArg1.value` emits smoke through active age 7; zero starts two
/// bouncing sparks and fading rings. Age 7 enters release; the next active tick
/// frees the work, body and task. Nonzero room effect control pauses below 4
/// and cancels at 4 or above. Keep this overlay and frame resources loaded;
/// spawned child effects have independent lifetimes.
void shelter1fParkingGarageRoomVisualEffectsSparkBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_PARKING_GARAGE_H
