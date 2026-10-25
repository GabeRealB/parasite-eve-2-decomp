#ifndef INCLUDE_ROOMS_NEO_ARK_BRIDGE_H
#define INCLUDE_ROOMS_NEO_ARK_BRIDGE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Four borrowed eight-point patrol rings selected by Diver spawn-argument bits 4..5.
///
/// Selectors 0 and 3 share a ring; 1 and 2 select the other two. X/Z are room
/// coordinates and Y is depth below `gNeoArkBridgeDiverWaterY`, all in game units.
/// Ring pointers remain valid only while this room overlay is loaded.
extern SVECTOR* gNeoArkBridgeDiverPatrolRings[4];

/// Writable emergence spots shared by the bridge's Divers.
///
/// Entries 1..7 supply room-coordinate X/Z; entry 0 is the reserved fallback
/// claim and is never searched. Y is unused by the searches. The fourth
/// halfword (`pad`) stores 0 free, 1 claimed; entry 8 has -1 and ends the list.
/// A Diver with no eligible spot retains its previous emergence position and
/// switches its claim index to 0. Claims are released on relocation or despawn.
/// Keep the room overlay loaded until all Divers have released their claims.
extern SVECTOR gNeoArkBridgeDiverSurfaceSpots[9];

/// Room-coordinate water-surface Y sampled by each bridge Diver at spawn.
///
/// Positive Y points downward. Patrol depths are added to this height; the
/// room overlay must remain loaded while the Diver configuration is read.
extern s16 gNeoArkBridgeDiverWaterY;

/// Bodyless water-refraction task recipe started on bridge entry.
///
/// One descriptor, priority `TASK_PRIORITY_CONTROL`, zero metadata, with no terminator. Spawn at
/// index 0 with zero arguments; the callback borrows the loaded room overlay.
extern TaskDesc gNeoArkBridgeWaterRefractionTaskDesc;

/// Placement/resource pairs indexed by the bridge's saved placement variant (0..12).
///
/// Slot 0 and slots 6..10 are empty; 1 is the default. Slots 1..5 pair Diver
/// layouts with room-owned resource recipes, sometimes adding Beetles or a
/// Mossback. Slots 11 and 12 supply Rook GOLEMs. Placements belong to the stage
/// map overlay; recipes belong to this room and borrow actor task descriptors.
/// Keep all owning overlays loaded while loading, spawning or updating actors.
extern AreaVariant gNeoArkBridgePlacementVariants[13];

/// Number of mapped camera/sprite entries and collision surface classes.
///
/// Camera/sprite capacity is independent of the room's reverse-search view count.
enum {
    NEO_ARK_BRIDGE_VIEW_RESOURCE_COUNT = 6,
    NEO_ARK_BRIDGE_SURFACE_CLASS_COUNT = 8,
};

/// The bridge's single room directory of logical-view byte maps.
///
/// Room 1 borrows `gViewIdentityMap`, indexed by logical view - 1, returning
/// the same 1-based resource index. The room's reverse-search count is 8;
/// camera and sprite lookups require a mapped index in 1..6. The directory
/// stays valid only while this room overlay is loaded; the map belongs to gameplay.
extern u8* gNeoArkBridgeViewMaps[1];

/// Room 1's signed logical-view reverse-search limit, eight entries.
///
/// The identity map provides those bytes, independently of the six camera,
/// sprite and view-file records. This count does not authorize a resource
/// lookup at mapped index 7 or 8. Borrowed by the stage map while this room is loaded.
extern ViewCount gNeoArkBridgeViewCounts[1];

/// Two bridge arrival/departure endpoints, indexed by the 1-based warp ID minus one.
///
/// Endpoint 1 is the pavilion end, at (-3160, 0, 6368), yaw 2048 and view 2;
/// endpoint 2 is the garden end, at (-3160, 0, -11600), yaw 0 and view 6.
/// Both place player and companion together in room-coordinate units, with
/// 4096 angle units per turn. They use bridge-bank entry 2 on arrival and 1
/// on departure, with no blocked sound, scripted spawn, fade or map flag.
/// Whole records are copied during transitions; keep this room loaded through lookup.
extern DirectionWarpEntry gNeoArkBridgeWarpEntries[2];

/// The bridge's room collision mesh and 3-by-7 XZ cell index.
///
/// Owns references to 5 normals, 97 vertices and 43 faces in this overlay.
/// Cells are 4000 game-coordinate units wide, from origin (-5100, -12200)
/// on X/Z, with X-major face lists terminated by `WORLD_COLLISION_GRID_CELL_END`.
/// Activation binds the mutable grid to the view; its pools stay live with this overlay.
extern WorldCollisionGrid gNeoArkBridgeCollisionGrid;

/// The bridge's six cameras, indexed by mapped view - 1.
///
/// Rotation has 12 fractional bits, translation is the negated world-space
/// camera origin, and screen distance is in pixels. Require a mapped index
/// in 1..6 independently of the logical reverse-search count. Queued cameras
/// borrow these records until application, so keep this overlay loaded.
extern ViewCamera gNeoArkBridgeViewCameras[NEO_ARK_BRIDGE_VIEW_RESOURCE_COUNT];

/// The bridge's six sprite views, indexed by mapped view - 1.
///
/// View 1 has no source sprites; views 2..6 select 34, 89, 135, 61 and 34
/// depth-sorted rectangles. Every first batch keeps decoded background strips;
/// every list ends with `SPRITE_BATCH_END`, and no view supplies clipping areas.
/// Require a mapped index in 1..6 and keep this overlay loaded while caching
/// or drawing the borrowed, mutable sources and batches.
extern SpriteView gNeoArkBridgeSpriteViews[NEO_ARK_BRIDGE_VIEW_RESOURCE_COUNT];

/// Four directional lights shared by every bridge view, with no point or cone lights.
///
/// The record borrows this overlay's mutable light array; coordinate setup and
/// shading update transforms and attenuation. Keep the overlay loaded while used.
extern WorldCoordRoomLights gNeoArkBridgeRoomLights;

/// Eight directed boundaries connecting the bridge's walking views 2 through 6.
///
/// Each adjacent pair has a boundary in each direction; the eighth record
/// carries `WORLD_COLLISION_TRIGGER_LAST`. Coordinates and radii use room
/// game units; normals use 4096 per unit. Room setup binds, links and enables
/// these mutable records. Keep this overlay loaded until they are unlinked.
extern WorldCollisionTrigger gNeoArkBridgeViewBoundaryTriggers[8];

/// Facing-gated passage and examination quads for the bridge.
///
/// The first requests pavilion arrival 2 via bridge endpoint 1; the second
/// requests garden arrival 1 via endpoint 2. The third starts CAP command 1
/// with presentation flags 0 and carries `WORLD_COLLISION_TRIGGER_LAST`.
/// Coordinates/radii use room game units and normals use 4096 per unit.
/// Room setup mutates links, transforms and hit state; keep the room overlay
/// loaded while linked and the caption bundle loaded for examination.
extern WorldCollisionTrigger gNeoArkBridgeActionTriggers[3];

/// Collision policies and footstep cues indexed by bridge surface class (0..7).
///
/// All classes apply pushback. Class 1 passes probes and suppresses weapon
/// impacts; every other class blocks probes and permits impacts. Only classes
/// 3 and 4 supply footstep pairs; 0, 2 and 5..7 share the silent solid record.
/// Records and cue bases are borrowed from this overlay and expire on unload.
extern WorldCollisionSurfaceProperties* gNeoArkBridgeSurfaceProperties[NEO_ARK_BRIDGE_SURFACE_CLASS_COUNT];

/// Installs bridge effect variants and updates its side-view smoke and red marker.
///
/// State 0 selects the room's five effect IDs, then state 1 updates mapped views
/// 5 and 6. Running effects independently try each smoke site with probability
/// 1/4 per tick; other controls suppress smoke. The red marker still draws in
/// both views, pulsing by 1536 angle units per animation frame (4096 per turn)
/// with radius scale 192. The task stays alive and uses no body or spawn payload.
/// Requires this overlay, room-effect state, view matrices, initialized scratch
/// stack and frame primitive space. Exported to gameplay's room-effect table.
void neoArkBridgeAmbientEffectsTask(Task* task);

/// Advances and draws an expanding, fading water-surface ripple for the bridge.
///
/// Requires a counted effect with a coordinate body, owned zeroed `EffectWork`
/// in `spawnArg2.pointer` and initial state 0. `spawnArg1` bits 0..11 give the
/// initial local half-side in coordinate units (0..4095); higher bits are ignored.
/// Each running update grows it by 32, draws, then dims by 2, for 32 draws at
/// brightness 64..2. The first update sets a random yaw, composed on the next.
/// Non-running controls redraw retained values; cancellation draws once before
/// retiring. Retirement frees the work and coordinate body and decrements the
/// effect count. The room overlay and water textures must stay loaded, with
/// initialized scratch storage and frame packet space available for drawing.
void neoArkBridgeWaterRippleTask(Task* task);

/// Advances and draws an eight-cell water-spray particle for the bridge.
///
/// Requires a counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, initial state 0 and animation index 0. `spawnArg1` bits
/// 0..11 give perspective size, 12..15 updates per cell (0 selects 1), and
/// 16..23 launch speed in parent-space coordinate units per update (0 selects 64).
/// Bits 24..27 choose direction (0 stationary, 1 upward burst, 2 all-axis spray,
/// 3 narrow upward jet, 5 spawn-offset direction; others leave it zero).
/// A supplied nonzero `EffectWork::move` bypasses direction generation and
/// scaling. Nonzero bits 28..31 select an upright tile; otherwise the sprite
/// retains a random angle at 4096 units per turn.
///
/// The first running update initializes without drawing or moving. Later
/// updates draw, move and add 6 to signed-halfword Y velocity, retaining its
/// low 16 bits; stationary particles skip movement and gravity. Cells 0..7
/// each last the decoded period. Suspended controls redraw retained state;
/// cancellation retires without drawing. Retirement frees the work and body
/// and decrements the effect count. The room overlay and water textures must
/// stay loaded, with initialized scratch storage and frame packet space.
void neoArkBridgeWaterDriftTask(Task* task);

/// Runs the bridge's charging pink flash, peak screen tint and fading star.
///
/// Requires a counted effect with a coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer` and initial state 0. `spawnArg1.value` starts as a
/// positive charge duration in running updates and is consumed as a countdown.
/// Nonzero room effect controls pause without drawing; controls 4 and above,
/// state 3 or fade completion retire the work, task and body and decrement
/// the effect count. The room overlay must remain loaded, with scratch and
/// frame packet storage available while drawing.
void neoArkBridgeRoomVisualEffectsFlashTask(Task* task);

/// Draws a fading blue beam between two offsets on the bridge effect's parent.
///
/// Requires a counted effect with a coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer`, initial state 0 and null `Task::work`. Its borrowed
/// `EffectWork::parent` coordinate and the room overlay must remain live.
/// Allocates eight world-space snapshots per endpoint in `Task::work`;
/// allocation failure resets age and retries on the next active update.
/// `spawnArg1.value` is the release age in active updates (2..32767 before
/// signed-halfword age wraps; 0 leaves lifetime to external teardown).
/// Initialization advances age without drawing or testing expiry. Control
/// values below 2 advance the trail; values 2 and above freeze without drawing
/// or canceling it. Teardown frees both histories, effect work and the body.
/// Drawing requires initialized scratch storage and frame packet space.
void neoArkBridgeRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the bridge's impact flash followed by smoke or rings and bouncing sparks.
///
/// Requires a counted effect with a coordinate body, owned zero-aged `EffectWork`
/// in `spawnArg2.pointer` and initial state 0. Nonzero `spawnArg1.value` selects
/// smoke; zero selects two sparks and fading orange rings. Running updates enter
/// release at age 7 and retire at age 8, freeing work, body and task. Child effects
/// live independently. Room control 1..3 pauses and 4 or above cancels immediately.
/// Keep the room overlay, effect resources, scratch and frame packet space live.
void neoArkBridgeRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs the Neo Ark bridge's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 idles and 2 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void neoArkBridgeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_BRIDGE_H
