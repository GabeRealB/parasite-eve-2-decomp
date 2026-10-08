#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_BALCONY_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_BALCONY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_motel_balcony_80186220[13];

// dryfield_motel_balcony
extern WorldCollisionRoomResources D_dryfield_motel_balcony_801822D0[];

/// Logical-view map directory for the balcony's single room.
///
/// Room 1 borrows the resident identity map: logical views 1..22 select the
/// same one-based camera/image/sprite indices. The room overlay owns this directory.
extern u8* gDryfieldMotelBalconyViewMaps[1];

extern ViewCount D_dryfield_motel_balcony_801822E4[];

/// Lighting descriptor for the balcony's single room.
///
/// Borrows nine mutable point lights and minimum ambient levels for 22 mapped
/// logical views. All room-owned pointers must remain live while lighting is queried.
extern WorldCoordRoomLighting gDryfieldMotelBalconyRoomLighting[1];

extern DirectionWarpEntry D_dryfield_motel_balcony_801822F0[];

extern ViewCamera D_dryfield_motel_balcony_80182B80[];

/// Sprite descriptors for balcony mapped views 1..22.
///
/// Indexed by mapped view minus one. The room owns mutable sources, terminated
/// batch lists and these descriptors for the lifetime of the loaded overlay.
extern SpriteView gDryfieldMotelBalconySpriteViews[22];

extern WorldCollisionSurfaceProperties* D_dryfield_motel_balcony_80186704[];

/// Runs this room's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is a positive charge duration in
/// active ticks, consumed as a countdown. Nonzero room effect control pauses
/// the task; control 4 or above, state 3, or completion releases the effect.
void dryfieldMotelBalconyRoomVisualEffectsFlashTask(Task* task);

/// Records two anchor-relative endpoints and draws their fading trail as a beam.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, whose `parent` stays live until teardown. `work` starts
/// null and owns sixteen allocated coordinate frames, eight per endpoint.
/// `spawnArg1.value` is the nonzero signed-16-bit age at which to finish; zero
/// leaves lifetime to external teardown. Age advances only with room effect
/// control below 2, including the successful initialization tick; allocation
/// failure resets it for retry. Teardown releases both allocations.
void dryfieldMotelBalconyRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the balcony's impact flash with smoke or orange rings and bouncing sparks.
///
/// Starts in state 0 with a coordinate body and a counted, owned, zero-aged
/// `EffectWork` in `spawnArg2.pointer`. Nonzero `spawnArg1.value` selects smoke;
/// zero selects sparks and rings. Active age 7 enters release and age 8 frees
/// the work and task. Nonzero room control below 4 pauses it; 4 or above cancels
/// it. Spawned effects run independently; keep their room callbacks loaded.
void dryfieldMotelBalconyRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs this room's vertically drifting animated mote until its brightness fades.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, with age and animation index initially zero.
/// `spawnArg1` packs a world-unit half-extent in bits 0..11, a palette selector
/// in bits 12..15 (0 default), unsigned speed in bits 16..23 in coordinate
/// units per active tick, and signed lifetime in bits 24..31 in active ticks.
/// The overlapping bits 0..1 select motion: either bit makes it steady,
/// bit 1 makes steady motion upward; otherwise it rises with added random speed.
/// The first active tick initializes without drawing; later ticks move along
/// local Y and draw on odd ages. Nonzero room effect control pauses it;
/// control 4 or above or completion releases the work and task.
void dryfieldMotelBalconyRoomVisualEffectsMoteTask(Task* task);

/// Runs this room's expanding tinted halo, shrinking ring and fading star.
///
/// Requires a coordinate body and a counted, owned, zero-initialized
/// `EffectWork` in `spawnArg2.pointer`; its borrowed parent must stay live.
/// Initialization attaches at the work's parent-relative offset in game units.
/// `spawnArg1.halves.low` is a nonzero unsigned expansion duration in active
/// ticks, and `halves.high` selects tint row 0..2. Initialization replaces the
/// argument word with the remaining expansion ticks. Nonzero room effect
/// control pauses it, including a state-3 release request; control 4 or above
/// or completion releases the work and task.
void dryfieldMotelBalconyRoomVisualEffectsHaloTask(Task* task);

/// Runs this room's expanding orange disc, layered glow and fading outer ring.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`; the spawn duration is unused. Nonzero room effect
/// control pauses the task; control 4 or above or completion releases it.
void dryfieldMotelBalconyRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty descending motes from turning offsets around the balcony effect.
///
/// Requires a coordinate body and a counted, owned, zero-aged `EffectWork` in
/// `spawnArg2.pointer`; `spawnArg1` is unused. Each active tick emits at local
/// Y = -128 * age and a turning radial offset near 768 game-coordinate units.
/// The motes descend by 8 units per active tick and run independently. Active
/// age 21 releases the emitter. Nonzero room control below 4 pauses it; 4 or
/// above cancels it. Keep this room and its installed mote callback loaded
/// until the emitter and all its motes finish.
void dryfieldMotelBalconyRoomVisualEffectsSparkEmitterTask(Task* task);

/// Installs this room's seven enemy-effect IDs once, then keeps the task idle.
///
/// State 0 selects the mote, halo, orange burst, spark emitter, flash,
/// twin-trail and spark-burst implementations for the gameplay room-effect
/// slots, then becomes state 1. Later ticks do nothing until external teardown.
void dryfieldMotelBalconyInitRoomEffectsTask(Task* task);

/// Dispatches the balcony's room receiver lifecycle.
///
/// `state` must be 0 (register messages), 1 (idle), or 2 (kill). Start in state
/// 0; initialization publishes the task in `GAME_TASK_SLOT_ROOM` and enters
/// state 1. The room's borrowed message and state tables must remain loaded
/// through teardown. Dispatch copies the complete three-entry state table.
void dryfieldMotelBalconyRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_BALCONY_H
