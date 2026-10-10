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

/// Logical-view maps for the hall's single room, indexed by room ID minus one.
///
/// Borrows `gViewIdentityMap`: logical views 1..7 select the same camera,
/// image and sprite indices. The map directory remains live while the room
/// overlay is loaded; it has no terminator.
extern u8* gShelterB2ElevatorHallViewMaps[1];

extern ViewCount D_shelter_b2_elevator_hall_801838E0[];

extern DirectionWarpEntry D_shelter_b2_elevator_hall_801838E4[];

/// The hall's wall and floor collision mesh, indexed by a 5-by-2 XZ cell grid.
///
/// Owns 7 normals, 48 vertices, 28 quad faces and 10 cell lists. Vertices and
/// 4000-unit cell sides use room game coordinates; unit normals use 4096.
/// Floor faces use surface classes 1 and 2, walls class 0. Activation binds
/// the view transform; mesh pools and lists remain writable and live with
/// this room overlay.
extern WorldCollisionGrid gShelterB2ElevatorHallRoomCollision;

extern ViewCamera D_shelter_b2_elevator_hall_80183DD8[];

/// Sprite and clipping resources for mapped hall views 1..7, indexed by view minus one.
///
/// Views 4 and 5 add foreground sprites to decoded background strips; view 4
/// also clips scene drawing. Other views have empty sprite lists. Borrows
/// mutable room-owned sources and batches for the lifetime of this overlay.
extern SpriteView gShelterB2ElevatorHallSpriteViews[7];

/// The hall's fourteen point lights, with no directional or cone lights.
///
/// Borrows writable room-owned entries for coordinate composition and light
/// attenuation. All entries contribute in every view; keep the overlay loaded.
extern WorldCoordRoomLights gShelterB2ElevatorHallRoomLights;

extern WorldCollisionTrigger D_shelter_b2_elevator_hall_801846CC[];

extern WorldCollisionOccluder D_shelter_b2_elevator_hall_8018492C[];

extern WorldCollisionTrigger D_shelter_b2_elevator_hall_80184968[];

/// Collision policies and footstep cues indexed by the hall's surface classes 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Floor
/// classes 1 and 2 use distinct footstep cue sets; 0 and 3..7 are silent.
/// The first policy byte is zero throughout, with its role unproven. Pointers
/// borrow scalar records owned by this overlay and expire when it unloads.
extern WorldCollisionSurfaceProperties* gShelterB2ElevatorHallSurfacePropertiesByClass[8];

/// Runs the elevator hall's room-message receiver through initialization, idle and teardown.
///
/// Requires a live task with state 0..2 (0 register receiver, 1 idle, 2 release).
/// Its borrowed message table and callbacks must remain loaded while it receives messages.
void shelterB2ElevatorHallRoomTask(Task* task);

/// Runs the elevator hall's pink charging flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `effectSpawn`. `spawnArg1.value` is a
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

/// Runs an impact flash followed by smoke or orange rings and bouncing sparks.
///
/// Start in state 0 with the coordinate body and zeroed, counted `EffectWork`
/// in `spawnArg2.pointer` supplied by `effectSpawn`. Nonzero `spawnArg1.value`
/// selects smoke; zero selects two sparks and fading rings. Active age seven
/// enters release; the next active tick frees work and kills the task.
/// Nonzero room effect control pauses it; four or above cancels it. Child effects
/// live independently. Keep the coordinate ancestors, effect controller, room
/// overlay and current graphics workspace live through teardown.
void shelterB2ElevatorHallRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs the elevator hall's animated, vertically drifting mote until its brightness fades.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `effectSpawn`; its coordinate parent must
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
/// in world units. The unsigned low half of `spawnArg1` holds expansion duration
/// 1..65535 in active ticks; the signed high half holds tint index 0..2.
/// Initialization replaces that word with the countdown. Nonzero room effect
/// control pauses it; four or above cancels it. Completion or cancellation
/// releases work and task.
void shelterB2ElevatorHallRoomVisualEffectsHaloTask(Task* task);

/// Runs the elevator hall's orange burst with an expanding glow and fading ring.
///
/// Requires a coordinate body and counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `effectSpawn`; its coordinate parent must
/// stay live. `spawnArg1` is unused. Nonzero room effect control pauses it;
/// four or above cancels it. Completion or cancellation releases work and task.
void shelterB2ElevatorHallRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty descending motes around its spawn coordinate.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; `spawnArg1` is unused. Active ages
/// 1..20 emit at turning radial offsets with local Y = -128 times age in
/// coordinate units. Motes descend eight units per active motion tick and
/// live independently after age 21 releases the emitter's work and task.
/// Nonzero room effect control pauses it; four or above cancels it. Requires
/// the installed mote effect; keep coordinate ancestors, controller and room
/// overlay live through the emitted effects' lifetimes.
void shelterB2ElevatorHallRoomVisualEffectsSparkEmitterTask(Task* task);

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
