#ifndef INCLUDE_ROOMS_NEO_ARK_ISLAND_H
#define INCLUDE_ROOMS_NEO_ARK_ISLAND_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Extents of the island's mapped view resources and collision surface directory.
enum {
    NEO_ARK_ISLAND_VIEW_RESOURCE_COUNT = 5,
    NEO_ARK_ISLAND_SURFACE_CLASS_COUNT = 8,
};

/// Extents of the Bog Diver's patrol-ring directory and emergence-spot list.
enum {
    NEO_ARK_ISLAND_DIVER_PATROL_RING_COUNT  = 4,
    NEO_ARK_ISLAND_DIVER_SURFACE_SPOT_COUNT = 7,
};

/// Four eight-point patrol rings selected by Bog Diver spawn-argument bits 4..5.
///
/// Selectors 0 and 3 share a ring; 1 and 2 select the other two. X/Z are room
/// coordinates and Y is depth below `gNeoArkIslandDiverWaterY`, all in game units.
/// The Diver borrows these pointers until teardown; keep the island loaded.
extern SVECTOR* gNeoArkIslandDiverPatrolRings[NEO_ARK_ISLAND_DIVER_PATROL_RING_COUNT];

/// Writable emergence spots shared by the island's Bog Divers.
///
/// Entries 1..5 supply room-coordinate X/Z; entry 0 is the reserved fallback
/// claim and is never searched. Y is unused by the searches. The fourth
/// halfword (`pad`) stores 0 free, 1 claimed; entry 6 has -1 and ends the list.
/// With no eligible spot, a Diver retains its position and selects claim 0.
/// Keep the island loaded until all Divers have released their claims.
extern SVECTOR gNeoArkIslandDiverSurfaceSpots[NEO_ARK_ISLAND_DIVER_SURFACE_SPOT_COUNT];

/// Room-coordinate water-surface Y sampled by each island Bog Diver at spawn.
///
/// Positive Y points downward; normal patrol adds waypoint depth to this height.
/// Keep the island loaded while the Diver configuration is read.
extern s16 gNeoArkIslandDiverWaterY;

/// Bodyless water-refraction task recipe started on island entry.
///
/// One descriptor, priority `TASK_PRIORITY_CONTROL`, zero metadata, with no terminator. Spawn at
/// index 0 with zero arguments; the callback borrows the loaded room overlay.
extern TaskDesc gNeoArkIslandWaterRefractionTaskDesc;

/// Number of retained placement-variant slots, including absent slots.
enum { NEO_ARK_ISLAND_AREA_VARIANT_COUNT = 13 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1, 2, 3, 4, 11 are present; every other slot is absent (NULL, NULL).
/// Slot 4 retains two terminator-only lists, distinct from an absent slot.
/// Supply an index in 0..12; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gNeoArkIslandAreaVariants[NEO_ARK_ISLAND_AREA_VARIANT_COUNT];

/// Collision resources for the single room, indexed by room minus one.
///
/// The single room borrows its grid, four view boundaries and three action triggers;
/// there are no sight occluders.
///
/// The stage map borrows this directory while the room overlay is loaded. Keep the grid
/// and its pools live while active, and the mutable trigger/occluder lists live until
/// unlinked. This directory has no terminator or bounds check.
extern WorldCollisionRoomResources gNeoArkIslandRoomCollisionResources[1];

/// Lighting for the island's single room selector, indexed by room minus one.
///
/// Borrows the island's four directional lights; a NULL ambient table selects
/// gameplay's default minimum ambient colour for every view. The stage map
/// borrows this descriptor and its light collection while the island is loaded.
extern WorldCoordRoomLighting gNeoArkIslandRoomLighting[1];

/// Logical-view map directory for the area's single room selector.
///
/// Entry zero borrows `gViewIdentityMap`: logical views 1..5 select the same
/// camera, image and sprite indices. Room IDs index at room minus one; there
/// is no terminator. The directory requires this room overlay to stay loaded.
extern u8* gNeoArkIslandViewMaps[1];

/// Neo Ark island logical-view search count for its single room.
///
/// Room 1 searches five identity-map entries, selecting mapped views 1..5.
/// Indexed by room - 1; signed halfword values count logical map elements,
/// independently of map capacity and mapped resource extents. No terminator.
/// Borrow the array only while this room overlay is loaded.
extern ViewCount gNeoArkIslandViewCounts[1];

/// Warp endpoint records for slots 1..2, indexed by warp minus one.
///
/// Slots 1 and 2 serve the Pavilion and Submarine Gallery connections, with arrival
/// views 2 and 4. Slot 2 retains the special keep-facing yaw word, fades on permitted
/// departure and has only an arrival sound. Neither has a map marker.
/// Keep-facing uses the current player yaw for departure; arrival spawns copy
/// the stored yaw's low halfword without resolving this special value.
///
/// Positions use signed whole world coordinates; yaw uses 4096 units per turn. The two
/// byte spans have unproven roles and remain zero. The stage map borrows this array;
/// keep the room overlay loaded through departure or arrival setup. Consumers copy
/// complete entries and actor spawns borrow their transforms synchronously. There is
/// no terminator or bounds check.
extern DirectionWarpEntry gNeoArkIslandWarpEntries[2];

/// The island's five cameras, indexed by mapped view minus one.
///
/// Mapped views 1..5 have no terminator; the room's identity map makes these
/// the logical views as well. Rotation has 12 fractional bits, translation is
/// the negated world-space camera origin, and screen distance is in pixels.
/// Queued camera tasks borrow these records until application; keep this overlay loaded.
extern ViewCamera gNeoArkIslandViewCameras[NEO_ARK_ISLAND_VIEW_RESOURCE_COUNT];

/// The island's five sprite views, indexed by mapped view minus one.
///
/// Views 1 and 5 have no source sprites; views 2..4 select 33, 68 and 140
/// depth-sorted image rectangles. Every first batch keeps decoded background
/// strips, every batch list ends at SPRITE_BATCH_END, and no view supplies
/// clipping areas. Require a mapped view in 1..5 and keep this overlay loaded
/// while caching or drawing the borrowed, mutable sources and batches.
extern SpriteView gNeoArkIslandSpriteViews[NEO_ARK_ISLAND_VIEW_RESOURCE_COUNT];

/// Collision policies and footstep cues indexed by island surface class (0..7).
///
/// All classes apply pushback. Class 1 passes probes and suppresses weapon
/// impacts; every other class blocks probes and permits impacts. Only classes
/// 3 and 4 supply footstep pairs; 0, 2 and 5..7 share the silent solid record.
/// The directory, records and cue bases expire when this overlay is unloaded.
extern WorldCollisionSurfaceProperties* gNeoArkIslandSurfaceProperties[NEO_ARK_ISLAND_SURFACE_CLASS_COUNT];

/// Installs the island's flash, twin-trail, spark-burst and water effect IDs once.
///
/// The room-effect controller spawns this counted coordinate-body task with
/// state 0. It publishes this overlay's effect IDs, then stays idle in state 1
/// until external teardown. Spawn arguments are unused; keep the room overlay
/// loaded while the task or the effects selected by these IDs are live.
void neoArkIslandInstallRoomEffectIdsTask(Task* task);

/// Advances an expanding, fading island water ripple using its cached draw transform.
///
/// Requires a live counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// one coordinate body with a composed `workm`, initial state 0 and age 0,
/// as `effectSpawn` supplies. Spawn bits 0..11 give the initial local half-side
/// (0..4095 coordinate units); higher bits are ignored. Half-side is stored in
/// `EffectWork::angle` and RGB brightness in `EffectWork::scale`.
///
/// Each running update ages, grows by 32, draws, then dims by 2. The first sets
/// brightness 64 and replaces local rotation with one random Y rotation,
/// preserving translation. The task never composes: its first draw uses the
/// cache before that yaw, and model draw passes refresh the linked coordinate.
/// A fresh ripple lasts 32 running updates, drawing brightness 64..2 and
/// half-sides from initial size + 32 through initial size + 1024 (32..5119).
///
/// Non-running control freezes initialization and aging but still draws;
/// control 4 or above draws once before cancellation. Completion and cancellation
/// release the work, counted task and body. Keep the overlay loaded and scratch
/// and frame packet storage available while live; released pointers must not be retained.
void neoArkIslandWaterRippleTask(Task* task);

/// Animates one eight-cell island water-spray particle using its cached draw coordinate.
///
/// Requires a live counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, initial state 0 and cell index 0, as `effectSpawn` supplies.
/// Drawers read the coordinate's composed `workm`; this task never composes it.
/// Movement changes local translation in parent-coordinate units and marks it
/// dirty, so drawing uses the cached position until another user composes it.
///
/// `spawnArg1` bits 0..11 give perspective size (0..4095), bits 12..15 running
/// updates per cell (0 selects 1), and bits 16..23 launch speed in parent units
/// per update (0 selects 64). Bits 24..27 select direction: 0 stationary,
/// 1 upward burst, 2 all-axis spray, 3 narrow upward jet, 5 the copied `pos`
/// offset; other values leave the zero direction for SDK normalization.
/// Nonzero bits 28..31 select the upright drawer; otherwise the sprite uses
/// a retained random angle in 4096 units per turn. A supplied nonzero `move`
/// bypasses direction generation and scaling; `step` becomes 64 to enable it.
///
/// The first running update initializes without drawing or moving. Later
/// updates draw cells 0..7 through u16 indices, move by signed halfword
/// velocity, and add 6 to Y velocity with halfword truncation. Stationary
/// particles skip movement and gravity. Each cell lasts `period` running
/// updates; retirement follows initialization plus 8 * `period` updates.
/// Non-running control freezes initialization and aging but still draws,
/// including once before cancellation at control 4 or above. Retirement frees
/// the work and tears down the counted task and body. Keep the overlay loaded
/// and scratch/frame packet storage available while live; released pointers
/// must not be retained.
void neoArkIslandWaterSprayTask(Task* task);

/// Runs the island's charging pink flash, peak screen tint and fading star.
///
/// Requires initial state 0, a coordinate body and a counted `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `effectSpawn`. `spawnArg1.value` starts
/// as a positive charge duration in running updates. The first update sets
/// the ramp; subsequent charge updates consume the argument as a countdown.
/// Nonzero room effect control pauses it; values 4 and above cancel it.
/// State 3 requests release on the next running update. Cancellation and
/// completion release the effect work, task and body. Keep the overlay loaded
/// and scratch/frame packet storage available while drawing.
void neoArkIslandRoomVisualEffectsFlashTask(Task* task);

/// Draws a fading blue ribbon between two offsets on the effect's moving parent.
///
/// Requires initial state 0, null `Task::work`, a coordinate body and counted
/// `EffectWork` with zero age in `spawnArg2.pointer`, as supplied by
/// `effectSpawn`. The borrowed `EffectWork::parent` and overlay must stay live.
/// Owns two eight-coordinate world-space histories in `Task::work`;
/// allocation failure resets age and retries on the next active update.
/// `spawnArg1.value` is the release age in active updates (2..32767 before
/// signed-halfword age wraps; 0 leaves lifetime to external teardown).
/// Initialization counts as the first update without drawing or testing expiry.
/// Room effect control below 2 advances it; values 2 and above freeze it
/// without drawing or cancellation. Teardown frees both histories, the effect
/// work and the body. Drawing requires scratch and frame packet storage.
void neoArkIslandRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the island's impact flash followed by smoke or orange rings and sparks.
///
/// Starts in state 0 with a coordinate body and counted, zero-aged `EffectWork`
/// in `spawnArg2.pointer` from `effectSpawn`. Nonzero `spawnArg1.value` selects
/// smoke; zero selects rings and bouncing sparks. Active age 7 requests release,
/// and the next active tick frees work and task. Child effects run independently.
/// Nonzero room effect control pauses it; values at least 4 cancel it. Keep the
/// room overlay and scratch/frame packet storage live while drawing.
void neoArkIslandRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs the island's room-message task.
///
/// Requires a live task in state 0..2: initialize the receiver, ambience and
/// CAP-completion sound cues; idle while messages handle requests; then
/// teardown. The room overlay must remain loaded through dispatch.
void neoArkIslandRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_ISLAND_H
