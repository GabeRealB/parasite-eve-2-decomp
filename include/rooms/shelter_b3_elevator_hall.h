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

/// Placement layouts for the hall, indexed by saved area variant 0..11.
///
/// Variant 1 has two Mad Chasers, 2 has two Amoebas and a large Mossback,
/// and 11 has two Stingers. Slots 0 and 3..10 have no placements or resources.
/// The directory has no terminator; keep the room loaded while its tables
/// are borrowed for loading, spawning or by live actors.
extern AreaVariant gShelterB3ElevatorHallAreaVariants[12];

/// Logical-view map directory for the hall's single room.
///
/// Room index zero borrows the first seven entries of `gViewIdentityMap`:
/// logical views 1..7 select the same 1-based camera, image and sprite indices.
/// There is no terminator. The directory requires this room overlay loaded;
/// the shared byte map remains owned by gameplay.
extern u8* gShelterB3ElevatorHallViewMaps[1];

/// Logical-view search count for the hall's single room.
///
/// Room 1 searches seven byte entries in its logical-view map. This signed
/// halfword array has one element and no terminator; it requires the loaded room.
extern ViewCount gShelterB3ElevatorHallRoomViewCounts[1];

/// The hall's three arrival and departure endpoints, indexed by warp ID minus one.
///
/// Slots 1..3 serve the incinerator control-room door, lower-sewer passage and
/// elevator respectively; their initial views are 2, 6 and 2. Positions use
/// world units and yaw uses 4096 per turn. There is no terminator. Lookups copy
/// the full entry and require this room overlay loaded.
extern DirectionWarpEntry gShelterB3ElevatorHallWarpEntries[3];

/// Collision mesh and XZ cell index for the hall's single room.
///
/// The 5-by-3 grid has 4000-world-unit cells and origin X/Z (-6978, -3041).
/// Its private pools contain 103 vertices, nine normals and 68 faces, whose
/// surface classes are 0..6. The X-major table borrows terminated face-index
/// lists. Room binding supplies the view coordinate; keep the record and all
/// its writable pools loaded while active.
extern WorldCollisionGrid gShelterB3ElevatorHallCollisionGrid;

/// Seven authored cameras for the hall's mapped views 1..7.
///
/// Indexed by mapped view minus one, with no terminator. `ViewCamera` gives
/// the world-to-view transform and pixel projection distance. Queued camera
/// requests borrow these records until application; keep the room loaded.
extern ViewCamera gShelterB3ElevatorHallViewCameras[7];

/// Foreground sprite resources for the hall's mapped views 1..7.
///
/// Indexed by mapped view minus one, with no descriptor terminator. Views
/// 3..6 contain source rectangles, terminated batch lists and clip lists;
/// views 1, 2 and 7 retain empty batch addresses in the source slot and draw
/// the decoded background. Descriptors and their lists require the room
/// overlay to remain loaded throughout packet allocation and drawing.
extern SpriteView gShelterB3ElevatorHallSpriteViews[7];

/// The hall's five authored point lights for model shading in every view.
///
/// Directional and cone arrays are absent. Point-light nodes remain writable
/// for parenting, composition and attenuation updates; the collection and its
/// arrays must remain in the loaded room overlay while lighting uses them.
extern WorldCoordRoomLights gShelterB3ElevatorHallRoomLights;

/// Eight directed view boundaries joining the hall's views 2, 3, 4, 5 and 6.
///
/// Opposing pairs request 2<->3, 3<->4, 4<->5 and 5<->6. The final live
/// record carries LAST. Room binding parents, links and enables these mutable
/// quads; keep their room-overlay storage alive until the list is cleared.
extern WorldCollisionTrigger gShelterB3ElevatorHallViewBoundaryTriggers[8];

/// Four action quads for the control-room door, descending stairs and elevator.
///
/// Record order is the control-room warp (hall slot 1 to area 41 slot 3),
/// a five-step descent on surface class 1 facing yaw 1024, the automatic
/// lower-sewer warp (hall slot 2 to area 43 slot 3), and the elevator request
/// (hall slot 3 to area 26 slot 1). The elevator handler resolves cab travel.
/// LAST marks the final live quad. Binding changes links, coordinate and enable
/// flags; collision changes hit latches. Keep the room loaded until unlinked.
extern WorldCollisionTrigger gShelterB3ElevatorHallActionTriggers[4];

/// Two room-space quads blocking line of sight across the hall.
///
/// The second live quad carries LAST. Room setup links and enables both;
/// their world-unit origins and corners are transformed by the current view.
/// Both retain authored low flag bit 0, whose role is unproven.
/// Keep this writable room-overlay storage loaded until the list is cleared.
extern WorldCollisionOccluder gShelterB3ElevatorHallOccluders[2];

/// Collision and footstep records indexed by the hall's surface class 0..7.
///
/// All eight pointers are populated, with no terminator. Classes 0 and 7 share
/// a record with no footstep cues. Classes 4 and 6 pass probes and ignore
/// weapon impacts; all classes apply grid pushback. Records and cue tables
/// are borrowed from this room overlay and require it to remain loaded.
extern WorldCollisionSurfaceProperties* gShelterB3ElevatorHallSurfacePropertiesByClass[8];

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
