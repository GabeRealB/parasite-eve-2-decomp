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

/// The breeding room's one-room directory of logical-view maps.
///
/// Room 1 borrows `gViewIdentityMap`: views 1..10 select the same 1-based
/// camera, image and sprite indices. There is no terminator. Keep the room
/// and gameplay overlays loaded while borrowing this directory and its map.
extern u8* gShelterB2BreedingRoomRoomViewMaps[1];

extern ViewCount D_shelter_b2_breeding_room_80180560[];

extern DirectionWarpEntry D_shelter_b2_breeding_room_80180564[];

/// The breeding room's collision mesh and XZ cell index.
///
/// Contains 75 faces in a 4-by-3 grid of 4000-unit square cells, with grid
/// origin (-890, -2602) in room XZ coordinates. Vertices use room units and normals
/// use 4096 for unit length. Activation binds the mutable descriptor to the
/// view coordinate. Its private mesh pools and cell lists live with the room.
extern WorldCollisionGrid gShelterB2BreedingRoomCollisionGrid;

extern ViewCamera D_shelter_b2_breeding_room_80181118[];

extern SpriteView D_shelter_b2_breeding_room_801833D4[];

/// The breeding room's nine authored point lights, without directional or cone lights.
///
/// All contribute in every logical view. Position and falloff radii use world
/// units; RGB intensity uses 4096 for full strength. Coordinate updates and
/// lighting queries mutate the private light array's transforms and attenuation.
/// Keep the collection and its array loaded while borrowing them.
extern WorldCoordRoomLights gShelterB2BreedingRoomRoomLights;

extern WorldCollisionTrigger D_shelter_b2_breeding_room_801837C4[];

extern WorldCollisionTrigger D_shelter_b2_breeding_room_80183F9C[];

/// Ambient-light minima for the breeding room: a count header and ten view colours.
///
/// Entry zero holds the count; logical views 1..10 index colours directly.
/// RGB minima use 16 units per 8-bit colour level. Views beyond the count use
/// gameplay's default minima. The table remains live with the room overlay.
extern WorldCoordRoomAmbientEntry gShelterB2BreedingRoomAmbientByView[11];

extern WorldCollisionOccluder D_shelter_b2_breeding_room_8018467C[];

/// Collision policies and footstep cues for the breeding room's surface classes 0..7.
///
/// Every class applies pushback. Class 2 passes probes and suppresses weapon
/// impacts; all other classes block probes and allow impacts. Classes 1, 3 and
/// 4 have distinct footstep cues; classes 0 and 5..7 share a silent record.
/// There is no terminator. The directory and private records live with the room.
extern WorldCollisionSurfaceProperties* gShelterB2BreedingRoomSurfacePropertiesByClass[8];

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

/// Runs an attached charge disc with player-joint sparks and a fading release ring.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; `spawnArg1.value` selects tint 0 or 1.
/// The initial state attaches at the work's copied offset in its borrowed
/// parent's axes. The owner may request flicker, release or cancellation with
/// `ROOM_VISUAL_EFFECTS_GLOW_DISC_*`. Growth emits sparks from player parts 3..18
/// every fourth active age and adopts them for teardown with the disc. Requires
/// the installed flying-spark effect, live player model and coordinate ancestors.
/// Nonzero room effect control pauses it; four or above cancels it. Completion
/// frees work, children and task; keep the controller and room overlay loaded.
void shelterB2BreedingRoomRoomVisualEffectsGlowDiscTask(Task* task);

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
