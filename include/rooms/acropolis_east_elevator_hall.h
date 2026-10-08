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

/// Draws one frame of a pulsing additive red diamond, then ends the counted effect.
///
/// Bank-6 slot 0x22 requires a live coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, distinct from `Task::work`. The low two bytes of
/// `spawnArg1` are unsigned pulse rate and size (0..255); its high half is ignored.
/// The frame counter times the rate wraps to an 8-bit phase folded into red
/// levels 0..128. Half extent is `size * 512 / depth` screen pixels.
///
/// Projects the composed translation through `GsWSMATRIX`, narrowing each
/// component to signed 16-bit game units. Depth is SZ3 / 4 (0..16383); values
/// below 17 queue nothing, and GTE FLAG is not tested. Requires initialized GTE
/// projection settings, one aligned `RoomGlowSpriteScratch` block and arena
/// room for two `POLY_G4`/`DR_TPAGE` pairs at accepted depth. The scaled depth
/// wraps into the current 1024-tag ordering table; packets live through GPU completion.
/// Scratch and effect work are released on every call. Keep this overlay loaded
/// through the callback; the room respawns its six beacons while view 2 is active.
void acropolisEastElevatorHallRedBeaconTask(Task* task);

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

/// Runs the east elevator hall's room messages and player debug display.
///
/// State 0 registers the receiver and initializes the placed actor; state 1
/// updates the player debug display; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisEastElevatorHallRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
