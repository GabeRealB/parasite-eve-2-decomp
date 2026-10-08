#ifndef INCLUDE_ROOMS_SHELTER_B1_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_SHELTER_B1_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_elevator_hall_80184940[12];

// shelter_b1_elevator_hall
extern WorldCoordRoomLighting D_shelter_b1_elevator_hall_80182DF8[];

extern WorldCollisionRoomResources D_shelter_b1_elevator_hall_80182E00[];

extern u8* D_shelter_b1_elevator_hall_80182E10[];

extern ViewCount D_shelter_b1_elevator_hall_80182E14[];

extern DirectionWarpEntry D_shelter_b1_elevator_hall_80182E18[];

extern ViewCamera D_shelter_b1_elevator_hall_80183438[];

extern SpriteView D_shelter_b1_elevator_hall_80183CC4[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_elevator_hall_801849D0[];

/// Registers the hall's room-message receiver, idles, then releases the room task.
///
/// Requires a live task initially in state 0. State 0 records the first visit
/// and updates the objective byte once; state 1 performs no frame work, leaving
/// the task available for messages; state 2 kills it. State must stay in 0..2:
/// dispatch copies all three handlers by value and does not check bounds.
/// Body and spawn arguments are unused. The room overlay and gameplay message
/// and game-flag services must remain loaded while the task is live.
void shelterB1ElevatorHallRoomTask(Task* task);

/// Runs a charging pink flash, peak screen tint and fading star.
///
/// Requires the coordinate body and counted, owned `EffectWork` from
/// `effectSpawn`. `spawnArg1.value` is a positive charge duration in active
/// ticks, consumed as a countdown. Nonzero room effect control pauses it;
/// four or above cancels it. State 3 also requests release. Completion or
/// cancellation releases work and task. Coordinate ancestors, the effect
/// controller and this room overlay must remain live.
void shelterB1ElevatorHallRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading blue twin trails.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`, a live parent coordinate and `Task::work` initially NULL.
/// Owns two eight-coordinate histories in `Task::work`; allocation failure
/// retries initialization. Snapshots retain world positions as the parent moves.
/// `spawnArg1.value` zero leaves the lifetime unlimited; values 2..32767
/// release at that age in active ticks. Control values 2 and above pause
/// updates, including release. Effect teardown frees both work allocations.
/// The effect controller and this room overlay must remain live until teardown.
void shelterB1ElevatorHallRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs a vertically drifting animated mote until it fades.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. `spawnArg1` packs the half-extent in bits 0..11, palette in
/// bits 12..15, unsigned speed in bits 16..23 and signed lifetime in bits
/// 24..31. Extent and speed use parent-coordinate units; lifetime uses active
/// ticks. Motion bits 0..1 overlap the extent: either selects steady motion,
/// with bit 1 selecting upward motion; neither selects a brightening rise
/// with added random speed. Initialization draws nothing; later ticks draw
/// on odd ages. Nonzero control pauses it; four or above cancels it.
/// Completion or cancellation releases work and task. Coordinate ancestors,
/// the effect controller and this room overlay must remain live.
void shelterB1ElevatorHallRoomVisualEffectsMoteTask(Task* task);

/// Runs an expanding tinted halo, shrinking ring and fading star.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. The unsigned low half of `spawnArg1` is a nonzero expansion
/// duration in active ticks; the signed high half selects tint row 0..2.
/// Initialization attaches at the saved local offset and replaces the argument
/// with its countdown. Nonzero control pauses it; four or above cancels it.
/// Completion or cancellation releases work and task. The borrowed parent,
/// effect controller and this room overlay must remain live.
void shelterB1ElevatorHallRoomVisualEffectsHaloTask(Task* task);

/// Emits twenty descending motes from rotating, successively higher offsets.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer`, supplied by `effectSpawn`; `spawnArg1` is unused.
/// Active ages 1..20 emit a mote around the spawn coordinate, with local Y
/// offset -128 times age in coordinate units. Motes descend by eight units
/// per motion tick and live independently after the emitter retires at age 21.
/// Nonzero room effect control pauses it; four or above cancels it. Retirement
/// or cancellation releases the counted work and task. The hall's mote effect
/// ID must be installed; coordinate ancestors, the effect controller and this
/// room overlay must remain live through the child effects' lifetimes.
void shelterB1ElevatorHallRoomVisualEffectsSparkEmitterTask(Task* task);

/// Installs the hall's actor-effect IDs and draws its visible capsule glows.
///
/// State 0 installs seven effect IDs once. Every tick draws additive glows
/// for mapped views 2..5 and 7..9; views 1 and 6 draw none. Requires this
/// loaded room, a composed view matrix, initialized scratch stack and current
/// frame packet arena and ordering table. Body and spawn arguments are unused;
/// the task stays live independently of room effect control.
void shelterB1ElevatorHallDrawGlowsTask(Task* task);

/// Runs an impact flash followed by smoke or orange rings and bouncing sparks.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer`, supplied by `effectSpawn`, and initial state 0.
/// Nonzero `spawnArg1.value` selects smoke; zero selects two sparks and two
/// fading rings. Active age seven enters state 3 for release; the next active
/// tick frees the counted work and kills the task. Nonzero room effect control
/// pauses it; four or above cancels it. Child effects live independently.
/// Coordinate ancestors, the effect controller, this room overlay and current
/// graphics workspace must remain live until teardown.
void shelterB1ElevatorHallRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs an expanding orange disc and layered glow inside a fading ring.
///
/// Requires the coordinate body and counted, owned `EffectWork` from
/// `effectSpawn`; `spawnArg1` is unused. The ring fades before the centre.
/// Nonzero control pauses it; four or above cancels it. Completion or
/// cancellation releases work and task. Coordinate ancestors, the effect
/// controller and this room overlay must remain live.
void shelterB1ElevatorHallRoomVisualEffectsHaloOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_ELEVATOR_HALL_H
