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

/// Runs the hall's attached charge disc, player-joint sparks and fading release ring.
///
/// Requires a coordinate body, zeroed counted `EffectWork` in `spawnArg2.pointer`
/// and initial state 0. `spawnArg1.value` selects tint 0 or 1. The work's parent
/// and ancestors must remain live; attachment uses its copied local position.
/// The owner selects grow, flicker, release or cancel through task state.
/// Growth emits adopted child sparks from live player model coordinates 3..18,
/// using the installed flying-spark callback. Room control pauses at nonzero
/// values below four and cancels at four or above. Completion releases work,
/// the body and child tasks. The effect callbacks and overlay must remain loaded.
void shelterB3ElevatorHallRoomVisualEffectsGlowDiscTask(Task* task);

/// Animates the elevator hall's flying spark along its initial target displacement.
///
/// Requires a counted single-coordinate effect task, initially in state 0
/// with age and frame index zero, and owned `EffectWork` in `spawnArg2.pointer`.
/// State 0 samples the composed world matrices of its
/// coordinate and the borrowed target `GfxCoord` in `spawnArg1.pointer`; the
/// target must remain live through that first active tick. The step is
/// 204/4096 of the initial displacement transformed into parent axes and
/// narrowed to signed 16-bit components. State 1 moves by the fixed step and
/// draws on odd ages. At age 20 it releases the work and
/// retires the task. Room effect control 0 advances it, nonzero values below
/// 4 pause it, and values of 4 or above cancel it. The room overlay must
/// remain loaded.
void shelterB3ElevatorHallRoomVisualEffectsFlyingSparkTask(Task* task);

/// Expands and fades the elevator hall's orange disc, layered glow and ring.
///
/// Requires a counted single-coordinate effect task with owned `EffectWork`
/// in `spawnArg2.pointer`, starting in state 0; `spawnArg1` is unused.
/// State 1 expands the glow while
/// fading the ring, then fades the disc, releases the work and retires the task.
/// Active draws also refresh a short-lived orange point light. Room effect
/// control 0 advances it, nonzero values below 4 pause it, and values of 4
/// or above cancel it. The room overlay must remain loaded.
void shelterB3ElevatorHallRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Runs the hall's animated, vertically drifting mote until its brightness fades.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `effectSpawn`; its coordinate parent must
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
/// `spawnArg2.pointer`, as supplied by `effectSpawn`; its coordinate parent must
/// stay live. `spawnArg1` is unused. Nonzero room effect control pauses it;
/// four or above cancels it. Completion or cancellation releases its work and task.
void shelterB3ElevatorHallRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty independent descending motes at rotating local offsets in the hall.
///
/// Requires a coordinate body, zero-aged counted `EffectWork` in
/// `spawnArg2.pointer` and live coordinate ancestors; `spawnArg1` is unused.
/// Active ages 1..20 emit through the installed mote callback; age 21 retires
/// the emitter. Motes descend eight parent-coordinate units per active tick
/// and can outlive it. Nonzero room control below four pauses the emitter;
/// four or above cancels it. Retirement releases work and the body. Keep the
/// overlay and both callbacks loaded while their tasks remain live.
void shelterB3ElevatorHallRoomVisualEffectsSparkEmitterTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B3_ELEVATOR_HALL_H
