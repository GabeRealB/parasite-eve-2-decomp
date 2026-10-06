#ifndef INCLUDE_ROOMS_SHELTER_B3_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_SHELTER_B3_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b3_elevator_hall_8018477C[12];

// shelter_b3_elevator_hall
extern u8* D_shelter_b3_elevator_hall_80182B54[];

extern ViewCount D_shelter_b3_elevator_hall_80182B58[];

extern DirectionWarpEntry D_shelter_b3_elevator_hall_80182B5C[];

extern WorldCollisionGrid D_shelter_b3_elevator_hall_801834C8;

extern ViewCamera D_shelter_b3_elevator_hall_801834EC[];

extern SpriteView D_shelter_b3_elevator_hall_801841DC[];

extern WorldCoordRoomLights D_shelter_b3_elevator_hall_80184410;

extern WorldCollisionTrigger D_shelter_b3_elevator_hall_80184428[];

extern WorldCollisionTrigger D_shelter_b3_elevator_hall_801847DC[];

extern WorldCollisionOccluder D_shelter_b3_elevator_hall_8018490C[];

extern WorldCollisionSurfaceProperties* D_shelter_b3_elevator_hall_801849E0[];

/// Runs the elevator hall's message receiver through initialization, idle and release.
///
/// Requires a live task with state 0..2 and this room overlay loaded. State 0
/// installs the handlers and registers `GAME_TASK_SLOT_ROOM`, then enters state 1.
/// State 1 waits for messages; state 2 kills the task. No work or body is required.
void shelterB3ElevatorHallRoomTask(Task* task);

/// Selects the hall's effect IDs and draws its fixed strip glows in the current view.
///
/// State 0 publishes the IDs once and advances to state 1. Mapped views 3, 4
/// and 6 each draw four glows; other views draw none. Requires the loaded room,
/// composed view matrix, scratch stack and current frame's primitive arena and
/// ordering table. Allocates no work and runs until external teardown;
/// room effect control does not pause it.
void shelterB3ElevatorHallDrawGlowsTask(Task* task);

void func_shelter_b3_elevator_hall_80180E18(Task* arg0);

void func_shelter_b3_elevator_hall_80181370(Task* task);

void func_shelter_b3_elevator_hall_80181FD0(Task* arg0);

/// Runs the hall's animated, vertically drifting mote until its brightness fades.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `Gp_SpawnEff`; its coordinate parent must
/// stay live. `spawnArg1.value` packs half-extent in world units (bits 0..11),
/// palette (12..15; 0 default), unsigned local Y speed per active tick (16..23),
/// and signed lifetime in active ticks (24..31). Overlapping bits 0..1 select
/// steady motion, with bit 1 upward; neither selects randomized upward motion.
/// Nonzero room effect control pauses it; four or above cancels it.
/// Completion or cancellation releases its work and task.
void shelterB3ElevatorHallRoomVisualEffectsMoteTask(Task* task);

/// Runs the hall's expanding tinted halo, contracting ring and fading star.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`. Its parent is borrowed and its position is a local
/// offset in world units. The unsigned low half of `spawnArg1` is expansion
/// duration 1..65535 in active ticks; its signed high half is tint index 0..2.
/// Initialization replaces that word with the countdown. Nonzero room effect
/// control pauses it; four or above cancels it. Completion or cancellation
/// releases its work and task; its coordinate parent must stay live.
void shelterB3ElevatorHallRoomVisualEffectsHaloTask(Task* task);

/// Runs the hall's orange burst with an expanding glow and fading ring.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `Gp_SpawnEff`; its coordinate parent must
/// stay live. `spawnArg1` is unused. Nonzero room effect control pauses it;
/// four or above cancels it. Completion or cancellation releases its work and task.
void shelterB3ElevatorHallRoomVisualEffectsHaloOrangeBurstTask(Task* task);

void func_shelter_b3_elevator_hall_80180CE4(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B3_ELEVATOR_HALL_H
