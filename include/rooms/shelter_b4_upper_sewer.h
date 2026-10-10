#ifndef INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H
#define INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "rooms/room_visual_effects.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Four patrol-ring selections borrowed by the upper sewer's Bog Divers.
///
/// Spawn bits 4..5 select index 0..3; indices 0, 2 and 3 share a ring, and
/// index 1 selects the other. Each ring has eight points without a terminator.
/// X/Z use world units; swimming adds the water Y sampled at spawn to waypoint Y.
/// The room and rings must outlive every borrowing diver.
extern SVECTOR* gShelterB4UpperSewerDiverPatrolRings[4];

/// Writable emergence spots shared by the upper sewer's Bog Divers.
///
/// Entry 0 is the fallback claim; entries 1..7 are searched by XZ distance.
/// The fourth halfword is 0 free, 1 claimed or -1 end (entry 8). Searches use
/// world-unit X/Z and ignore Y. Divers release claims when moved or torn down;
/// keep the list loaded and writable until every borrowing diver releases it.
extern SVECTOR gShelterB4UpperSewerDiverSurfaceSpots[9];

/// Upper-sewer water-surface Y in world units, also sampled by Bog Diver spawns.
///
/// Room setup seeds -1800 after the reservoir event, or 0 without water drawing.
/// The first water-task tick publishes that seed; later drawing clamps to -1600..0.
/// Storage belongs to the loaded room and must outlive its borrowing divers.
extern s16 gShelterB4UpperSewerWaterY;

/// Placement/resource layouts for the upper sewer, indexed by the saved variant.
///
/// Slots 1, 2, 3, 4, 7, 8 and 11 are populated; 0, 5, 6, 9 and 10 are absent.
/// Each populated slot borrows room-owned lists ending at `AREA_PLACEMENT_END`.
/// Actors may retain placement pointers until teardown; keep this overlay loaded.
extern AreaVariant gShelterB4UpperSewerAreaVariants[12];

// shelter_b4_upper_sewer
/// The upper sewer's single-room directory of logical-view mappings.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..14 select the same
/// 1-based camera, image and sprite indices. The directory has no terminator
/// and remains valid while this room overlay is loaded.
extern u8* gShelterB4UpperSewerViewMaps[1];

/// Reverse-lookup view count for the upper sewer's single room.
///
/// Room 1 searches fourteen logical-view entries in `gShelterB4UpperSewerViewMaps`.
/// The signed-halfword count array has no terminator and is borrowed by the
/// Shelter map only while this room overlay is loaded.
extern ViewCount gShelterB4UpperSewerViewCounts[1];

/// Upper-sewer warp endpoints 1..4, indexed by warp ID minus one without a terminator.
///
/// Endpoints 1 and 4 lead to the lower sewer; 2 leads to the reservoir and
/// requests a departure fade and map marker; 3 leads to the water supply.
/// Arrival views are 7, 2, 10 and 12. Positions use world units and yaw uses
/// 4096 per turn. The stage directory borrows these records while the room is loaded.
extern DirectionWarpEntry gShelterB4UpperSewerWarpEntries[4];

/// The upper sewer's collision mesh and 6-by-5 XZ cell index.
///
/// Cells have 4000-world-unit sides and origin (-10010, -6000). The grid
/// borrows 7 normals, 87 vertices and 42 faces; face surface classes select
/// `gShelterB4UpperSewerSurfacePropertiesByClass`. Activation binds its view
/// coordinate. The mutable record and pools require this overlay to stay loaded.
extern WorldCollisionGrid gShelterB4UpperSewerCollisionGrid;

/// Camera transforms and projection distances for upper-sewer mapped views 1..14.
///
/// Index by mapped view minus one; there is no terminator. Rotations use
/// 4096 per unit, translations are negated world-space camera origins, and
/// projection distances are pixels. Queued camera tasks borrow these records
/// until application, so this room overlay must remain loaded until then.
extern ViewCamera gShelterB4UpperSewerViewCameras[14];

/// Sprite directory for upper-sewer mapped views 1..14, indexed by view minus one.
///
/// Borrows mutable room-owned sources and terminated batch lists, without
/// clipping lists. Views 1, 6, 7, 10 and 14 have no foreground sprites; all
/// views retain the decoded background. The water-hole scene toggles view 13's
/// single foreground sprite through its batch. Keep this overlay loaded while
/// the directory or any borrowed list is in use.
extern SpriteView gShelterB4UpperSewerSpriteViews[14];

/// The upper sewer's collection of 19 point lights, admitted in every view.
///
/// There are no directional or cone lights. The room owns the mutable point
/// array; coordinate updates parent it to the view and shading overwrites its
/// attenuation. The collection and array remain valid while this overlay is loaded.
extern WorldCoordRoomLights gShelterB4UpperSewerRoomLights;

/// Eighteen directed camera-change boundaries for the upper sewer's single room.
///
/// Each source/destination pair is a 1-based logical view; the final live
/// boundary carries `WORLD_COLLISION_TRIGGER_LAST`. Geometry uses world units
/// and normals use 4096 per unit. Room setup binds the view coordinate and
/// mutates links, enable flags and hit latches; keep the overlay loaded until
/// the runtime view-boundary list is cleared.
extern WorldCollisionTrigger gShelterB4UpperSewerViewBoundaryTriggers[18];

/// Eight upper-sewer interaction quads, ending at the final live trigger's LAST bit.
///
/// Four resolve warps to water supply, reservoir and lower sewer; the others
/// run the water-hole command outside battle or CAP commands 3, 5 and 4.
/// Positions use world units and normals use 4096 per unit. Room setup binds
/// the view coordinate, links and enables these mutable records. Keep them
/// loaded until the runtime action list is cleared.
extern WorldCollisionTrigger gShelterB4UpperSewerActionTriggers[8];

/// Three upper-sewer line-of-sight blocking quads, ending at the final live LAST bit.
///
/// Positions and radii use world units; normals use 4096 per unit. Room setup
/// links and enables the mutable records. Their low flag bit has no established
/// interpretation. Keep the room loaded until the runtime occluder list is cleared.
extern WorldCollisionOccluder gShelterB4UpperSewerOccluders[3];

/// Collision policies and footstep cues indexed by the upper sewer's surface class.
///
/// All eight slots apply pushback. Class 1 passes probes and ignores weapon
/// impacts; the others block probes and allow impacts. Classes 2, 3 and 5
/// have footstep cues; 0, 1, 4, 6 and 7 are silent. There is no terminator.
/// Records and cue bases are borrowed only while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelterB4UpperSewerSurfacePropertiesByClass[8];

/// Dispatches the upper sewer's room setup, message-only idle state and teardown.
///
/// Start in state 0 with this room loaded. Setup installs the room's message
/// table and task slot, then advances to state 1. A nonzero reservoir-event flag
/// also starts water rendering at height -1800 world units; zero sets height 0.
/// State 2 kills the task. Only states 0..2 are valid; the task has no body and
/// ignores its packed spawn-location argument. The room must stay loaded.
void shelterB4UpperSewerRoomTask(Task* task);

/// Runs the sewer's charging pink flash, peak screen tint and fading star.
///
/// Needs a counted effect with a coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, starting in state 0. `spawnArg1.value` is a positive
/// charge duration in running ticks, consumed as a countdown. Nonzero room
/// effect control pauses; four or above cancels. Completion frees work and task.
void shelterB4UpperSewerRoomVisualEffectsFlashTask(Task* task);

/// Records and draws the sewer's two-ended trail from eight world-space snapshots.
///
/// Needs a counted effect, coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer`, initial state 0 and NULL `task->work`. The work's
/// parent must stay live. Initialization allocates the two histories and
/// retries on failure. `spawnArg1.value` is the nonzero age at which to retire;
/// zero gives no age limit. Control values below `ROOM_EFFECT_CONTROL_HIDDEN`
/// advance the trail; other values stop it. Teardown frees work and histories.
void shelterB4UpperSewerRoomVisualEffectsTwinTrailTask(Task* task);

/// Emits the sewer's impact flash with smoke puffs or sparks and fading rings.
///
/// Needs a counted effect with a coordinate body, owned zero-aged `EffectWork`
/// in `spawnArg2.pointer` and initial state 0. Nonzero `spawnArg1.value` selects
/// smoke; zero selects two independent bouncing sparks and two orange rings.
/// Running age 7 enters release; age 8 frees work and task. Nonzero effect
/// control pauses; four or above cancels. Spawned effects outlive this task.
void shelterB4UpperSewerRoomVisualEffectsSparkBurstTask(Task* task);

/// Installs the sewer's effect IDs once and draws glow capsules for the mapped view.
///
/// Start in state 0 with this room loaded. Ripple and spray IDs are installed
/// only when `GAME_FLAG_B4_RESERVOIR_EVENT_DONE` is exactly 1. Other IDs are
/// always installed. Subsequent ticks draw without consulting effect control;
/// this task does not retire itself or use its coordinate body or spawn arguments.
void shelterB4UpperSewerDrawGlowsTask(Task* task);

/// Grows and fades one ripple on the sewer's water surface.
///
/// Needs a counted effect with a coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer` and initial state 0. `spawnArg1` bits 0..11 give the
/// initial local half-side in world units; higher bits are ignored. Running
/// ticks grow it by 32 and fade brightness from 64 by 2, for 32 draws.
/// Paused ticks redraw; cancellation redraws once before freeing work and task.
void shelterB4UpperSewerWaterRippleTask(Task* task);

/// Animates one sewer water-spray particle, with optional movement and gravity.
///
/// Needs a counted effect, coordinate body, owned zero-initialized `EffectWork`
/// in `spawnArg2.pointer` and initial state 0. `spawnArg1` packs size in bits
/// 0..11, ticks per cell in 12..15 (0 means 1), speed in 16..23 (0 means 64),
/// velocity kind in 24..27 (0 stationary, 1 upward burst, 2 spray, 3 narrow jet,
/// 5 direction from work's `pos`), and upright mode in 28..31 (nonzero selects
/// a tile). Supplied nonzero `move` bypasses velocity generation. Movement
/// uses parent-space units per running tick; gravity adds 6 to Y velocity.
/// Paused ticks redraw; cancellation or eight-cell completion frees work and task.
void shelterB4UpperSewerWaterDriftTask(Task* task);

/// Runs the sewer's attached charge disc, player-joint sparks and fading release ring.
///
/// Needs a counted effect with a coordinate body, owned zeroed `EffectWork`
/// in `spawnArg2.pointer` and initial `ROOM_VISUAL_EFFECTS_GLOW_DISC_ATTACH` state.
/// `spawnArg1.value` selects tint 0 or 1. The work's parent coordinate and its
/// ancestors must stay live; `pos` is the offset in its axes, in world units. Growth
/// emits adopted flying sparks from player model parts 3..18 every fourth age,
/// requiring the player model and installed flying-spark callback to stay live.
/// The owner requests flicker, release or cancel through the glow-disc states.
/// Nonzero effect control pauses; four or above cancels. Release or cancellation
/// frees work, task and adopted sparks.
void shelterB4UpperSewerRoomVisualEffectsGlowDiscTask(Task* task);

/// Flies the sewer's animated spark along a step fixed from its initial target offset.
///
/// Needs a counted effect, coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, starting in state 0. `spawnArg1.pointer` borrows a target
/// `GfxCoord`; both world matrices must be composed for initialization. The
/// target is sampled only then, deriving a parent-space step of 204/4096 of
/// the offset. Nonzero effect control pauses, four or above cancels, and age
/// 20 frees work and task. The target pointer must stay valid through initialization.
void shelterB4UpperSewerRoomVisualEffectsFlyingSparkTask(Task* task);

/// Draws the sewer's flying-section orange disc, layered glow and fading ring.
///
/// Needs a counted effect, coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, starting in state 0. Spawn argument 1 is unused.
/// Running ticks expand the glow and ring, fade the ring, then fade the centre.
/// Nonzero effect control pauses; four or above cancels. Completion frees work and task.
void shelterB4UpperSewerRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Moves and fades one animated sewer mote along its coordinate's local Y axis.
///
/// Needs a counted effect, coordinate body and owned zero-initialized
/// `EffectWork` in `spawnArg2.pointer`, starting in state 0. `spawnArg1` bits
/// 0..11 give world-unit half-extent, with overlapping bits 0..1 selecting
/// steady motion (bit 1 upward); 12..15 select the palette, 16..23 unsigned
/// speed, and 24..31 signed lifetime in running ticks. Initialization does
/// not draw. Nonzero effect control pauses; four or above cancels.
/// Completion frees work and task after the brightness fades to zero.
void shelterB4UpperSewerRoomVisualEffectsMoteTask(Task* task);

/// Expands the sewer's tinted halo and shrinking ring, then fades a star.
///
/// Needs a counted effect, coordinate body and owned zero-initialized
/// `EffectWork` in `spawnArg2.pointer`, starting in state 0. The work's parent
/// must stay live; `pos` is a parent-space offset in world units. Signed
/// `spawnArg1` halves give positive expansion ticks (low) and tint row 0..2
/// (high). The argument becomes a countdown. Nonzero effect control pauses;
/// four or above cancels. Completion frees work and task.
void shelterB4UpperSewerRoomVisualEffectsHaloTask(Task* task);

/// Draws the sewer's halo-section orange disc, layered glow and fading ring.
///
/// Needs a counted effect, coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, starting in state 0. Spawn argument 1 is unused.
/// Running ticks expand the glow and ring, fade the ring, then fade the centre.
/// Nonzero effect control pauses; four or above cancels. Completion frees work and task.
void shelterB4UpperSewerRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty motes at rotating, rising offsets around the sewer effect's coordinate.
///
/// Needs a counted effect with a coordinate body and owned zero-aged `EffectWork`
/// in `spawnArg2.pointer`; state and `spawnArg1` are unused. Running ages 1..20
/// emit independent motes at about 768 local radial units and Y = -128 * age.
/// Each mote descends eight parent-axis units per running tick. The room's mote
/// ID and callback must be installed and loaded. Age 21 frees work and task.
/// Nonzero effect control pauses; four or above cancels. Motes outlive the emitter.
void shelterB4UpperSewerRoomVisualEffectsSparkEmitterTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H
