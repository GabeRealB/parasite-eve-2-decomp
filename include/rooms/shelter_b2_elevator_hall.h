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

/// Extents of the hall's exported room, camera, warp, collision and placement tables.
enum {
    SHELTER_B2_ELEVATOR_HALL_ROOM_COUNT                  = 1,
    SHELTER_B2_ELEVATOR_HALL_VIEW_COUNT                  = 7,
    SHELTER_B2_ELEVATOR_HALL_WARP_ENTRY_COUNT            = 3,
    SHELTER_B2_ELEVATOR_HALL_VIEW_BOUNDARY_TRIGGER_COUNT = 8,
    SHELTER_B2_ELEVATOR_HALL_SIGHT_OCCLUDER_COUNT        = 1,
    SHELTER_B2_ELEVATOR_HALL_WARP_TRIGGER_COUNT          = 3,
    SHELTER_B2_ELEVATOR_HALL_AREA_VARIANT_COUNT          = 22,
};

/// Encounter layouts indexed by the hall's placement variant, 0..21.
///
/// Populated slots are 1 (seven Maggots), 2 (two Brain Stingers), 3 (three
/// large Mossbacks), 11 (four Watchers and a patrolling launcher Rook
/// GOLEM), and 21 (two Watchers and two launcher Rook GOLEMs). Other slots
/// contain two NULL pointers. Each populated slot borrows room-owned resource
/// and placement lists ending at `AREA_PLACEMENT_END`; actors keep placement
/// pointers, so the overlay must remain loaded until those actors are released.
extern AreaVariant gShelterB2ElevatorHallAreaVariants[SHELTER_B2_ELEVATOR_HALL_AREA_VARIANT_COUNT];

/// Logical-view maps for the hall's single room, indexed by room ID minus one.
///
/// Borrows `gViewIdentityMap`: logical views 1..7 select the same camera,
/// image and sprite indices. The map directory remains live while the room
/// overlay is loaded; it has no terminator.
extern u8* gShelterB2ElevatorHallViewMaps[1];

/// Reverse-lookup view counts indexed by room minus one; the hall's only room has seven views.
///
/// Counts signed logical-view entries in `gShelterB2ElevatorHallViewMaps`, with
/// no terminator. The map overlay borrows this storage while the hall is loaded.
extern ViewCount gShelterB2ElevatorHallRoomViewCounts[SHELTER_B2_ELEVATOR_HALL_ROOM_COUNT];

/// Arrival placements and departure options for the hall's three warp endpoints.
///
/// Indexed by warp minus one: 1 main corridor (view 2), 2 elevator (view 5),
/// 3 south maintenance walkway (view 6). Player and companion share each
/// signed world-unit placement and 4096-unit yaw; each endpoint has its own
/// arrival/departure sounds and map flag, and endpoint 3 adds a blocked sound.
/// There is no terminator. The map overlay borrows these records while loaded;
/// transition consumers copy a complete `DirectionWarpEntry` by value.
extern DirectionWarpEntry gShelterB2ElevatorHallWarpEntries[SHELTER_B2_ELEVATOR_HALL_WARP_ENTRY_COUNT];

/// The hall's wall and floor collision mesh, indexed by a 5-by-2 XZ cell grid.
///
/// Owns 7 normals, 48 vertices, 28 quad faces and 10 cell lists. Vertices and
/// 4000-unit cell sides use room game coordinates; unit normals use 4096.
/// Floor faces use surface classes 1 and 2, walls class 0. Activation binds
/// the view transform; mesh pools and lists remain writable and live with
/// this room overlay.
extern WorldCollisionGrid gShelterB2ElevatorHallRoomCollision;

/// Camera transforms and projection distances for mapped hall views 1..7.
///
/// Indexed by mapped view minus one, with no terminator. Rotation uses twelve
/// fractional bits, translation is the negated world-space camera origin, and
/// projection distances are pixels. View 1 looks down; views 2..7 look along
/// the hall. Queued camera tasks borrow a record until applying it, requiring
/// this overlay to remain loaded through application.
extern ViewCamera gShelterB2ElevatorHallViewCameras[SHELTER_B2_ELEVATOR_HALL_VIEW_COUNT];

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

/// Eight directed quads changing hall views in pairs 2/3, 3/4, 4/5 and 5/6.
///
/// Origins, corner offsets and radii use room game units; normals use 4096 per
/// unit. The final live quad carries `WORLD_COLLISION_TRIGGER_LAST`. Room setup
/// binds the view coordinate, links and enables every record. Links and hit
/// latches are mutable; keep the room overlay loaded until the array is unlinked.
extern WorldCollisionTrigger gShelterB2ElevatorHallViewBoundaryTriggers[SHELTER_B2_ELEVATOR_HALL_VIEW_BOUNDARY_TRIGGER_COUNT];

/// One room-space quad blocking enemy sight across the hall.
///
/// Positions and radius use game units; the normal uses 4096 per unit. The
/// sole live record carries `WORLD_COLLISION_OCCLUDER_LAST`; low flag bit 0
/// and the final zero byte have unproven roles. Setup links and enables it.
/// Its links are mutable and borrow room storage until the occluder is unlinked.
extern WorldCollisionOccluder gShelterB2ElevatorHallSightOccluders[SHELTER_B2_ELEVATOR_HALL_SIGHT_OCCLUDER_COUNT];

/// Facing-gated warp quads for the main corridor, elevator and south walkway.
///
/// Each uses hall endpoint 1, 2 or 3 respectively and destination arrival 1.
/// Room-unit origins, corners and radii define the overlap regions; normals
/// use 4096 per unit. The final live quad carries `WORLD_COLLISION_TRIGGER_LAST`.
/// Setup binds the view coordinate, links and enables the writable records;
/// the hall overlay must remain loaded until their links and hit latches retire.
extern WorldCollisionTrigger gShelterB2ElevatorHallWarpTriggers[SHELTER_B2_ELEVATOR_HALL_WARP_TRIGGER_COUNT];

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
