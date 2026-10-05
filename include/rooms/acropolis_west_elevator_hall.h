#ifndef INCLUDE_ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// acropolis_west_elevator_hall
extern WorldCollisionRoomResources D_acropolis_west_elevator_hall_80185024[];

extern u8* D_acropolis_west_elevator_hall_80185034[];

extern ViewCount D_acropolis_west_elevator_hall_80185038[];

extern WorldCoordRoomLighting D_acropolis_west_elevator_hall_8018503C[];

extern DirectionWarpEntry D_acropolis_west_elevator_hall_80185044[];

extern SpriteView D_acropolis_west_elevator_hall_80186408[];

extern ViewCamera D_acropolis_west_elevator_hall_801869FC[];

extern WorldCollisionSurfaceProperties* D_acropolis_west_elevator_hall_80186AC4[];

void acropolisWestElevatorHallRedBeaconTask(Task* arg0);

/// Distorts an 82-row, 120-pixel strip of the drawing framebuffer for one frame.
///
/// Copies rows at (80, 80) in the selected buffer, whose VRAM Y stride is 272.
/// A cosine selects each source X; alternate 128-frame intervals vary its
/// divisor randomly per row. Reserves 82 DR_MOVE packets, all in ordering-table
/// element 0x72. Requires a current table and enough packet-arena space.
/// Releases the counted EffectWork in `spawnArg2.pointer` and kills the task
/// after queuing the copies; packets remain live through GPU completion.
void acropolisWestElevatorHallScanlineDistortionTask(Task* task);

/// Draws one frame of the hall's additive light billboard, then retires its effect.
///
/// Requires a live coordinate body, a counted EffectWork in `spawnArg2.pointer`,
/// the current view matrix, and room for one POLY_FT4 and RoomGlowSpriteScratch.
/// The composed position narrows to signed 16-bit view coordinates before projection.
/// Depths below 17 (camera Z / 4) reserve a packet but do not queue it; other
/// depths give a half-extent of 0x6700 / depth pixels. The scratch block is
/// released before effect teardown. Queued storage lives through GPU completion.
void acropolisWestElevatorHallLightGlowTask(Task* task);

/// Lights the elevator bay's palette over two updates and holds it in view 5.
///
/// `spawnArg2.pointer` owns a counted EffectWork initialized with zero scale
/// and angle. Here scale is the Q12 lit-palette weight (0..4096), and angle is
/// the latch (0 ramping, 1 holding). Each ramp step adds 2048 and uploads all
/// 256 colours. Leaving view 5 restores the unlit palette before freeing the
/// work and killing the task, including when a ramp upload happened that tick.
void acropolisWestElevatorHallBayLightingTask(Task* task);

/// Runs this room's player-model floor or mirror-plane reflection.
///
/// Starts bodyless in state 0 with a live player TMD; `spawnArg1.value` selects
/// floor (0) or room-plane (1). Initialization owns a cloned model and mirror
/// work, parents the task to the player, spawns attachment reflections and
/// performs the first update. State 1 follows the player's pose, lighting and
/// reflected frame. Requires this room overlay to remain loaded; dispatch
/// accepts only states 0 and 1 and does not check bounds.
void acropolisWestElevatorHallPlayerReflectionTask(Task* reflectionTask);

void func_acropolis_west_elevator_hall_8017F7D4(Task* task);

void func_acropolis_west_elevator_hall_8017F5F4(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H
