#ifndef INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b2_elevator_hall_80184C7C[22];

// shelter_b2_elevator_hall
extern u8* D_shelter_b2_elevator_hall_801838DC[];

extern ViewCount D_shelter_b2_elevator_hall_801838E0[];

extern DirectionWarpEntry D_shelter_b2_elevator_hall_801838E4[];

extern WorldCollisionGrid D_shelter_b2_elevator_hall_80183DB4;

extern ViewCamera D_shelter_b2_elevator_hall_80183DD8[];

extern SpriteView D_shelter_b2_elevator_hall_80184120[];

extern WorldCoordRoomLights D_shelter_b2_elevator_hall_801846B4;

extern WorldCollisionTrigger D_shelter_b2_elevator_hall_801846CC[];

extern WorldCollisionOccluder D_shelter_b2_elevator_hall_8018492C[];

extern WorldCollisionTrigger D_shelter_b2_elevator_hall_80184968[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_elevator_hall_80184D5C[];

/// Runs the elevator hall's room-message receiver through initialization, idle and teardown.
///
/// Requires a live task with state 0..2 (0 register receiver, 1 idle, 2 release).
/// Its borrowed message table and callbacks must remain loaded while it receives messages.
void shelterB2ElevatorHallRoomTask(Task* task);

/// Runs the elevator hall's pink charging flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `Gp_SpawnEff`. `spawnArg1.value` is a
/// positive charge duration in active ticks, consumed as a countdown. Nonzero
/// room effect control pauses the task; four or above cancels it. Completion
/// or cancellation releases its work and task; its parent coordinate must stay live.
void shelterB2ElevatorHallRoomVisualEffectsFlashTask(Task* task);

/// Records eight world-space snapshots of two moving endpoints and draws their connecting trail.
///
/// Requires a coordinate body and counted `EffectWork` in `spawnArg2.pointer`,
/// with age zero and a live borrowed parent coordinate. State 0 allocates and
/// seeds both histories in `Task::work`; state 1 records and draws them. The
/// two endpoints use the room's local offsets from that parent. Control 0 or 1
/// advances the task; values 2 and above suspend it, including cancellation values.
/// `spawnArg1.value` is the stopping age in active ticks, tested after drawing
/// in state 1 (0 never stops); it must be reachable by the signed 16-bit age.
/// Completion or external teardown releases the histories, effect work and task.
void shelterB2ElevatorHallRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_b2_elevator_hall_80182B48(Task* task);

/// Runs the elevator hall's animated, vertically drifting mote until its brightness fades.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `Gp_SpawnEff`; its coordinate parent must
/// stay live. `spawnArg1.value` packs half-extent in world units (bits 0..11),
/// palette (12..15; 0 default), unsigned speed in local Y units per active tick
/// (16..23), and signed lifetime in active ticks (24..31). The overlapping bits
/// 0..1 select steady motion, with bit 1 upward; neither selects randomized upward
/// motion. Nonzero room effect control pauses it; four or above cancels it.
/// Completion or cancellation releases its work and task.
void shelterB2ElevatorHallRoomVisualEffectsMoteTask(Task* task);

/// Runs the elevator hall's expanding tinted halo, contracting ring and fading star.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`. Its parent is borrowed and its position is a local offset
/// in world units. The signed halves of `spawnArg1` hold a positive expansion
/// duration in active ticks (low) and tint index 0..2 (high). Initialization
/// replaces that word with the countdown. Nonzero room effect control pauses
/// it; four or above cancels it. Completion or cancellation releases work and task.
void shelterB2ElevatorHallRoomVisualEffectsHaloTask(Task* task);

/// Runs the elevator hall's orange burst with an expanding glow and fading ring.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `Gp_SpawnEff`; its coordinate parent must
/// stay live. `spawnArg1` is unused. Nonzero room effect control pauses it;
/// four or above cancels it. Completion or cancellation releases work and task.
void shelterB2ElevatorHallRoomVisualEffectsHaloOrangeBurstTask(Task* task);

void func_shelter_b2_elevator_hall_801816C8(Task* arg0);

/// Selects the hall's effect task IDs and draws lamp and door-indicator glows for the current view.
///
/// State 0 selects the IDs once and advances to state 1; every invocation draws
/// views 2..6 from the mapped camera's low byte, with no glows in other views.
/// Views 5 and 6 select the south walkway door indicator from its unlock flag.
/// Requires the room overlay, composed view matrix, scratch stack and current
/// frame's primitive arena and ordering table. It allocates no task work and
/// continues until external teardown; room effect control does not pause it.
void shelterB2ElevatorHallDrawGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_HALL_H
