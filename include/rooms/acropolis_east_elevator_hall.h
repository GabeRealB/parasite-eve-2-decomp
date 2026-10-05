#ifndef INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_acropolis_east_elevator_hall_80186C50[3];

// acropolis_east_elevator_hall
extern WorldCollisionRoomResources D_acropolis_east_elevator_hall_80186320[];

extern u8* D_acropolis_east_elevator_hall_80186330[];

extern ViewCount D_acropolis_east_elevator_hall_80186334[];

extern WorldCoordRoomLighting D_acropolis_east_elevator_hall_80186338[];

extern DirectionWarpEntry D_acropolis_east_elevator_hall_80186340[];

extern SpriteView D_acropolis_east_elevator_hall_80187870[];

extern ViewCamera D_acropolis_east_elevator_hall_80187A5C[];

extern WorldCollisionSurfaceProperties* D_acropolis_east_elevator_hall_80187B74[];

void func_acropolis_east_elevator_hall_8017F5B4(Task* task);

void acropolisEastElevatorHallRedBeaconTask(Task* arg0);

/// Initializes and updates the player's floor or mirror-plane reflection in this room.
///
/// State must be 0 (start bodyless) or 1 (initialized model and work).
/// `spawnArg1.value` selects the floor (0) or room mirror plane (1).
/// Requires a live player with a TMD body. The task owns its cloned model and
/// work, borrows the player's geometry, and becomes a child of the player.
/// Keep this overlay and the player live while the reflection and its
/// attachment reflections run.
void acropolisEastElevatorHallPlayerReflectionTask(Task* reflectionTask);

/// Draws one additive gray pixel at the effect coordinate, then ends the effect.
///
/// Bank-6 slot 0x23 requires a live coordinate body and a counted room effect.
/// Its composed translation narrows to signed 16-bit game coordinates before
/// projection through `GsWSMATRIX`. Only SZ3 / 4 depths at least
/// `EFFECT_POINT_TILE_MIN_DEPTH` queue the tile; GTE FLAG is not tested.
/// The frame's word-aligned packet arena needs room for one `TILE_1` and, at
/// an accepted depth, one `DR_TPAGE`; packets remain live until GPU drawing.
///
/// Every call frees `spawnArg2.pointer` and tears down the task, even when
/// nothing is drawn. That argument must be NULL or an owned primary-heap
/// allocation, distinct from `Task::work`, with nested resources released.
/// Keep this overlay loaded through the callback.
void acropolisEastElevatorHallPointTileTask(Task* task);

void func_acropolis_east_elevator_hall_8017F55C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
