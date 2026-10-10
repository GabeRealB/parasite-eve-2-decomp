#ifndef INCLUDE_ROOMS_SHELTER_B1_MAIN_CORRIDOR_H
#define INCLUDE_ROOMS_SHELTER_B1_MAIN_CORRIDOR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Saved placement variants 0..21 for the Shelter B1 main corridor.
///
/// Variant 1 is the default; 2, 3, 7 and 11 select alternate actor layouts.
/// Every other slot has NULL lists, including variant 21's empty corridor.
/// Gameplay borrows the selected placement/resource lists, so keep this room
/// overlay loaded while loading, spawning or using its placement records.
extern AreaVariant gShelterB1MainCorridorAreaVariants[22];

/// Logical-view maps for the corridor's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..10 select the same
/// 1-based camera, image and sprite indices. The directory has no terminator;
/// keep this room overlay and gameplay loaded while using it.
extern u8* gShelterB1MainCorridorRoomViewMaps[1];

/// Ten logical views in the corridor's single room, indexed by room ID minus one.
///
/// Limits reverse searches of the room's identity view map; no terminator.
/// The Shelter map borrows this signed-halfword count while the room is loaded.
extern ViewCount gShelterB1MainCorridorRoomViewCounts[1];

/// Six departure and arrival endpoints, indexed by 1-based warp slot minus one.
///
/// Slots 1..6 lead to the elevator hall, control-room access tunnel, armory,
/// transfer tunnel, sleeping quarters and sterilization room. Each stores
/// player/companion world placements, arrival view and sounds; slot 4 also
/// supplies the locked-door sound. There is no terminator. The Shelter map
/// borrows the table; keep this room loaded until the complete entry is copied.
extern DirectionWarpEntry gShelterB1MainCorridorWarpEntries[6];

/// The corridor's room-space collision mesh and 3-by-6 XZ cell index.
///
/// Owns 34 normals, 172 vertices and 82 faces; cells are 4000 world units wide,
/// starting at X = -5398, Z = -19715. Faces use surface classes 0..2 from
/// `gShelterB1MainCorridorSurfacePropertiesByClass`. Activation binds the view
/// coordinate; keep this overlay loaded while the grid and its pools are active.
extern WorldCollisionGrid gShelterB1MainCorridorCollisionGrid;

/// Cameras for mapped corridor views 1..10, indexed by mapped view minus one.
///
/// Matrices use 4096 per unit and negated origins in signed world coordinates;
/// projection distances are pixels. No terminator. The map and queued camera
/// task borrow these records until application, so the room must stay loaded.
extern ViewCamera gShelterB1MainCorridorViewCameras[10];

/// Sprite resources for mapped corridor views 1..10, indexed by mapped view minus one.
///
/// Views 1..3 retain empty lists and decoded backdrops. Views 4..10 select
/// complete sprite-source ranges; views 4 and 7 also select depth clips.
/// The map borrows this mutable table and its lists while the room is loaded;
/// cached packets require the records through initialization and drawing.
extern SpriteView gShelterB1MainCorridorSpriteViews[10];

/// Six point lights shading models throughout the corridor's views.
///
/// No directional or cone lights. Positions and falloff radii are world units;
/// RGB intensities have 12 fractional bits. Gameplay updates the borrowed
/// transforms and attenuation, so the record and room overlay must remain live.
extern WorldCoordRoomLights gShelterB1MainCorridorRoomLights;

/// Sixteen directed view-change quads for the corridor's single room.
///
/// Source/destination view bytes are 1-based views 2..10. The final live quad
/// carries WORLD_COLLISION_TRIGGER_LAST. Positions and radii are game units,
/// normals use 4096 per unit. Gameplay binds and links the mutable records;
/// keep the room loaded until they are unlinked.
extern WorldCollisionTrigger gShelterB1MainCorridorViewBoundaries[16];

/// Six facing-gated door quads requesting departures from the corridor.
///
/// In warp-slot order: elevator hall, control-room access tunnel, armory,
/// transfer tunnel, sleeping quarters and sterilization room. Each packs its
/// departure slot in the high nibble and destination arrival slot in the low
/// nibble; the final live quad carries WORLD_COLLISION_TRIGGER_LAST. Positions
/// and radii are game units, normals use 4096 per unit. Gameplay binds and
/// links the mutable records; keep the room loaded until they are unlinked.
extern WorldCollisionTrigger gShelterB1MainCorridorWarpTriggers[6];

/// Collision policies and footstep cues indexed by room-local surface class 0..7.
///
/// Class 1 passes probes and disables weapon impacts; class 2 is the floor and
/// supplies footsteps. Class 0 and classes 3..7 share blocking, impact-enabled
/// properties without footstep cues. All classes apply pushback. No terminator;
/// keep the room overlay loaded while gameplay borrows these records.
extern WorldCollisionSurfaceProperties* gShelterB1MainCorridorSurfacePropertiesByClass[8];

/// Runs the corridor's room-message receiver through registration, idle and teardown.
///
/// Requires state 0..2 for its three-entry dispatch table. State 0 registers
/// the receiver, state 1 waits for messages and state 2 kills it. Keep the room
/// and Shelter map overlays loaded while the task and message table are live.
void shelterB1MainCorridorRoomTask(Task* task);

/// Installs the room's effect IDs once and draws the lights in the active view.
///
/// Starts in state 0 and remains in state 1 after installing the IDs. Mapped
/// view indices 2..10 select fixed world-space beams and flares; others draw none.
/// Requires current view matrices, frame packet storage and the glow drawers'
/// scratch capacity. The task and room overlay stay live while it is dispatched.
void shelterB1MainCorridorDrawViewLightsTask(Task* task);

/// Runs the room's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` supplied through
/// `spawnArg2.pointer` by `effectSpawn`. `spawnArg1.value` is a positive charge
/// duration in active ticks, consumed as a countdown. Nonzero room effect
/// control pauses the task; values 4 and above cancel it. Completion or
/// cancellation releases the counted work and task; state 3 requests release.
void shelterB1MainCorridorRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading twin-trail beam.
///
/// Requires a coordinate body, zeroed, counted `EffectWork` from `effectSpawn`
/// and `Task::work` initially NULL. Borrows the spawn parent for its lifetime
/// and owns two eight-coordinate histories in `Task::work`; allocation failure
/// retries initialization. `spawnArg1.value` is zero for an unlimited lifetime,
/// or 2..32767 for the age in active ticks at which to release. Room effect
/// control values 2 and above pause updates. Normal teardown releases both
/// the counted work and histories.
void shelterB1MainCorridorRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the corridor's impact flash with smoke puffs or fading rings and bouncing sparks.
///
/// Requires a coordinate body and zero-aged, counted `EffectWork` from
/// `effectSpawn` in `spawnArg2.pointer`, starting in state 0. Nonzero
/// `spawnArg1.value` selects smoke; zero selects rings and two sparks. Enters
/// release at active age 7 and frees work and task on the next active tick.
/// Spawned effects have independent lifetimes. Nonzero room effect control
/// pauses updates; four or above cancels. Coordinate ancestors, the effect
/// controller and this room overlay must remain live until teardown.
void shelterB1MainCorridorRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs the room's vertically drifting animated mote until its brightness fades.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` packs world-unit
/// half-extent in bits 0..11, palette in bits 12..15, unsigned vertical speed
/// in bits 16..23 and signed lifetime in active ticks in bits 24..31.
/// Bits 0..1 overlap the extent: either selects steady motion, with bit 1
/// upward; neither selects a rising mote with added random speed. Nonzero room
/// effect control pauses it; values 4 and above cancel it. Completion or
/// cancellation releases the counted work and task.
void shelterB1MainCorridorRoomVisualEffectsMoteTask(Task* task);

/// Runs the room's expanding tinted halo and shrinking ring, then a fading star.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`; its parent stays live and `pos` is the parent-space offset.
/// The unsigned low half of `spawnArg1` holds an expansion duration of 1..65535
/// active ticks; the signed high half selects tint index 0..2. Initialization
/// replaces the word with the remaining duration. Nonzero room effect control
/// pauses it; values 4 and above cancel it. State 3 requests release; teardown
/// frees the counted work.
void shelterB1MainCorridorRoomVisualEffectsHaloTask(Task* task);

/// Runs the room's growing orange disc and glow within an expanding, fading ring.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; `spawnArg1` is unused. The ring
/// fades before the central burst. Nonzero room effect control pauses it;
/// values 4 and above cancel it. Completion or cancellation frees the counted
/// work and task.
void shelterB1MainCorridorRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty motes at rotating, successively higher offsets around its coordinate.
///
/// Requires a coordinate body and zero-aged, counted `EffectWork` from
/// `effectSpawn` in `spawnArg2.pointer`; `spawnArg1` is unused. Each active
/// age 1..20 emits an independent mote that descends eight parent-axis units
/// per tick; age 21 releases the emitter's work and task. Requires the room's
/// mote selector to be installed. Nonzero room effect control pauses updates;
/// four or above cancels. Coordinate ancestors and the effect controller stay
/// live through teardown; this room overlay stays loaded while emitted motes run.
void shelterB1MainCorridorRoomVisualEffectsSparkEmitterTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_MAIN_CORRIDOR_H
