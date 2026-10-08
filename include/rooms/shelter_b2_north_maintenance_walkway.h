#ifndef INCLUDE_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H
#define INCLUDE_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b2_north_maintenance_walkway_80186258[22];

// shelter_b2_north_maintenance_walkway
extern u8* D_shelter_b2_north_maintenance_walkway_80183C5C[];

extern ViewCount D_shelter_b2_north_maintenance_walkway_80183C60[];

extern DirectionWarpEntry D_shelter_b2_north_maintenance_walkway_80183C64[];

extern WorldCollisionGrid D_shelter_b2_north_maintenance_walkway_8018401C;

extern ViewCamera D_shelter_b2_north_maintenance_walkway_80184040[];

extern SpriteView D_shelter_b2_north_maintenance_walkway_80185B04[];

extern WorldCoordRoomLights D_shelter_b2_north_maintenance_walkway_80185D44;

extern WorldCollisionTrigger D_shelter_b2_north_maintenance_walkway_80185D5C[];

extern WorldCollisionTrigger D_shelter_b2_north_maintenance_walkway_80185F24[];

extern WorldCollisionOccluder D_shelter_b2_north_maintenance_walkway_80186308[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_north_maintenance_walkway_80186360[];

void func_shelter_b2_north_maintenance_walkway_8017DD90(Task* task);

/// Runs this room's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is a positive charge duration in
/// active ticks, consumed as a countdown. Nonzero room effect control pauses
/// it; control 4 or above, state 3, or completion releases the work and task.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task);

/// Records two parent-relative endpoints and draws their fading twin trail.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, with a parent that stays live until teardown. `work`
/// starts NULL and owns two eight-frame world-space coordinate histories after
/// initialization. `spawnArg1.value` is the nonzero signed-16-bit stopping age,
/// tested only on later recording ticks; zero leaves lifetime to external
/// teardown. Age advances with room effect control below 2; control 2 or above
/// pauses without cancelling. Failed history allocation resets age for retry.
/// Teardown releases both work allocations and the coordinate body.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs this room's impact flash followed by smoke or fading orange spark rings.
///
/// Start at state 0 with a coordinate body and counted, zero-aged `EffectWork`
/// owned through `spawnArg2.pointer`. Nonzero `spawnArg1.value` emits seven
/// smoke puffs; zero emits two bouncing sparks and draws fixed/expanding rings.
/// Active age 7 enters release; age 8 releases work and task. Children are
/// independent. Room-effect control 1..3 pauses and 4 or above cancels. Keep
/// this room, coordinate ancestors, effect resources, scratch and GPU arena live
/// until teardown; caller work pointers become invalid when the effect ends.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs this room's vertically drifting animated mote until its brightness fades.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, with age and animation index initially zero.
/// `spawnArg1` packs a game-unit half-extent in bits 0..11, a palette selector
/// in bits 12..15 (0 default), unsigned speed in bits 16..23 in parent-space
/// units per active tick, and a signed lifetime in bits 24..31 in active ticks.
/// Bits 0..1 overlap the extent: either selects steady motion, with bit 1
/// selecting upward motion; otherwise it rises with added random speed 0..63.
/// The first active tick only initializes; later ticks draw on odd ages and
/// fade by 16 once age exceeds lifetime minus eight. Nonzero room effect
/// control pauses it; control 4 or above or completion releases work and task.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsMoteTask(Task* task);

/// Runs this room's expanding tinted halo, shrinking ring and fading star.
///
/// Requires a coordinate body and a counted, owned, zero-initialized
/// `EffectWork` in `spawnArg2.pointer`; its parent must stay live. Initialization
/// attaches at the work's parent-relative offset in game units.
/// `spawnArg1.halves.low` is a positive unsigned expansion duration in active
/// ticks; `halves.high` selects tint row 0..2. Initialization replaces the word
/// with the remaining ticks. Nonzero room effect control pauses every phase,
/// including state 3's release request; control 4 or above or completion
/// releases the work and task.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloTask(Task* task);

/// Runs this room's expanding orange disc, layered glow and fading outer ring.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`; `spawnArg1` is unused. Nonzero room effect control
/// pauses it; control 4 or above or completion releases the work and task.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloOrangeBurstTask(Task* task);

void func_shelter_b2_north_maintenance_walkway_80181A80(Task* arg0);

/// Installs this room's enemy-effect IDs and draws its view-specific light glows.
///
/// State 0 installs the seven effect IDs and becomes state 1; every tick draws
/// the glows for mapped camera indices 2..8. Other views draw nothing. The
/// operating-room north-door flag selects an orange locked-door glow or a blue
/// unlocked-door glow in views 2 and 7. Requires the room overlay and current
/// view/rendering state to stay live; retains the task until external teardown.
void shelterB2NorthMaintenanceWalkwayGlowTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H
