#ifndef INCLUDE_ROOMS_SHELTER_B2_BREEDING_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B2_BREEDING_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB2BreedingRoomModel02E04;

// shelter_b2_breeding_room
extern u8* D_shelter_b2_breeding_room_8018055C[];

extern ViewCount D_shelter_b2_breeding_room_80180560[];

extern DirectionWarpEntry D_shelter_b2_breeding_room_80180564[];

extern WorldCollisionGrid D_shelter_b2_breeding_room_801810F4;

extern ViewCamera D_shelter_b2_breeding_room_80181118[];

extern SpriteView D_shelter_b2_breeding_room_801833D4[];

extern WorldCoordRoomLights D_shelter_b2_breeding_room_801837AC;

extern WorldCollisionTrigger D_shelter_b2_breeding_room_801837C4[];

extern WorldCollisionTrigger D_shelter_b2_breeding_room_80183F9C[];

extern WorldCoordRoomAmbientEntry D_shelter_b2_breeding_room_80184624[];

extern WorldCollisionOccluder D_shelter_b2_breeding_room_8018467C[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_breeding_room_801847F4[];

extern AreaVariant D_shelter_b2_breeding_room_80183EEC[22];

/// Updates the placed breeding-room model's visibility from its object state.
///
/// Requires a live `TASK_BODY_TMD` body and borrowed `Enemy` work in
/// `spawnArg2.pointer`. The low byte of its placement key must be an object-state
/// index in 0..63 for the current stage. State 2 suppresses active drawing;
/// states 0, 1 and 3 allow it. No model or work ownership changes.
void shelterB2BreedingRoomAreaObjectTask(Task* task);

/// Runs the breeding-room controller's setup, idle or teardown state.
///
/// Requires a live bodyless task with state 0..2; dispatch is unchecked.
/// State 0 publishes the message table and enables CAP-completion sound cues,
/// then advances to state 1. State 1 waits for messages; state 2 kills the task.
/// The room and `map_shelter` overlays must remain loaded through message
/// handling.
void shelterB2BreedingRoomRoomTask(Task* task);

void func_shelter_b2_breeding_room_8017E774(Task* arg0);

/// Runs the Breeding Room's animated spark along its initial target displacement.
///
/// Requires a counted `effectSpawn` task with a `TASK_BODY_COORD` body and owned
/// `EffectWork` in `spawnArg2.pointer`. `spawnArg1.pointer` borrows a target
/// `GfxCoord`; both world matrices must be composed on the first active tick.
/// That tick fixes a flight step in parent axes using a Q12 factor of 204;
/// the target is not sampled again. Draws on odd ages and releases the work
/// at age 20. Room effect control 0 runs, other values below 4 pause, and
/// values at least 4 cancel and release the work.
void shelterB2BreedingRoomRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the Breeding Room's expanding orange burst, ground glow and fading ring.
///
/// Requires a counted `effectSpawn` task with a `TASK_BODY_COORD` body and owned
/// `EffectWork` in `spawnArg2.pointer`; `spawnArg1` is unused. The glow grows
/// by 16 world units per active tick. The expanding ring fades first, then
/// the central burst fades and releases the work. Each draw refreshes the
/// shared transient orange point light. Room effect control 0 runs, other
/// values below 4 pause, and values at least 4 cancel and release the work.
void shelterB2BreedingRoomRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Draws the breeding room's fixed disc and capsule glows for the active camera.
///
/// Bank-6 slot 0x13F. A live task starting at state 0 installs this room's
/// glow-disc, flying-spark and orange-burst effect IDs, then advances to state 1.
/// Draws on that first tick and every later tick for mapped camera indices 2..6;
/// other indices emit no glow packets. The room overlay must remain loaded.
/// Requires the current view transform, an initialized scratch stack and space
/// in the current frame's GPU packet arena.
void shelterB2BreedingRoomDrawGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_BREEDING_ROOM_H
