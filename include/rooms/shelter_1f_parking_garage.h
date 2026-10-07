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

/// Task entries the Neo Ark map UI overlay's stage tables name: each room's
/// entry task, started for its location, and the enemy descriptors' tasks.
void func_shelter_1f_parking_garage_8017DF14(Task* task);

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

void func_shelter_1f_parking_garage_8017FF58(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_PARKING_GARAGE_H
