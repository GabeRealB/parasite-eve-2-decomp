#ifndef INCLUDE_ROOMS_ACROPOLIS_PATIO_H
#define INCLUDE_ROOMS_ACROPOLIS_PATIO_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Extents of the patio's exported room, view, warp and collision-class tables.
enum {
    ACROPOLIS_PATIO_VARIANT_SLOT_COUNT  = 12,
    ACROPOLIS_PATIO_ROOM_COUNT          = 3,
    ACROPOLIS_PATIO_VIEW_COUNT          = 19,
    ACROPOLIS_PATIO_WARP_COUNT          = 4,
    ACROPOLIS_PATIO_SURFACE_CLASS_COUNT = 8,
};

/// Actor placements and loading/spawn recipes indexed directly by placement variant.
///
/// Slots 1..5 and 10 pair map-owned placements with patio-owned resources;
/// slots 0, 6..9 and 11 are absent. All twelve slots are retained storage, not a
/// terminated list. Callers must supply an index in 0..11 and keep the patio,
/// Acropolis map and referenced actor packages loaded through loading/spawning.
extern AreaVariant gAcropolisPatioAreaVariants[ACROPOLIS_PATIO_VARIANT_SLOT_COUNT];

/// Collision resources for patio rooms 1..3, indexed by room - 1.
///
/// All rooms share one grid, fourteen view boundaries and two sight blockers.
/// Room 1 selects its own twelve action triggers; rooms 2 and 3 share another
/// twelve. Setup mutates the borrowed grid and list records. Keep the patio
/// overlay loaded until the resources are replaced and its lists unlinked.
extern WorldCollisionRoomResources gAcropolisPatioRoomCollisionResources[ACROPOLIS_PATIO_ROOM_COUNT];

/// Logical-view maps for patio rooms 1..3, indexed by room - 1.
///
/// Each map has 19 logical views, addressed at view - 1, with 1-based
/// camera/image/sprite indices. Room 1 borrows `gViewIdentityMap`; rooms 2 and 3
/// map logical view 4 to index 17, and room 3 also maps view 7 to index 13.
/// Other entries are identity mappings. Map directories and private byte maps
/// live with the loaded patio overlay; the gameplay-owned identity map must
/// also remain loaded. Maps remain writable through the shared view interface.
extern u8* gAcropolisPatioViewMaps[ACROPOLIS_PATIO_ROOM_COUNT];

/// Signed logical-view search counts for patio rooms 1..3, indexed by room - 1.
///
/// Each count is 19 elements for reverse lookup in `gAcropolisPatioViewMaps`.
/// This is the active logical extent, independent of the maps' storage sizes.
/// The map overlay borrows these halfwords while the patio overlay is loaded.
extern ViewCount gAcropolisPatioViewCounts[ACROPOLIS_PATIO_ROOM_COUNT];

/// Lighting descriptors for patio rooms 1..3, indexed by room - 1.
///
/// All entries borrow the same 24-point-light collection and use the gameplay
/// default ambient minima. The patio overlay owns the descriptors and mutable
/// lights; borrowed pointers must not outlive the loaded overlay.
extern WorldCoordRoomLighting gAcropolisPatioRoomLightingTable[ACROPOLIS_PATIO_ROOM_COUNT];

/// Patio arrival/departure endpoints, indexed by 1-based warp ID minus one.
///
/// IDs 1 and 4 are the square-side stair endpoints; 2 is the fountain door and
/// 3 is the cafeteria door. ID 4 places the player and companion separately
/// and starts the player in scripted mode. Door endpoints supply sounds and
/// map-marker IDs. There is no terminator; the map overlay borrows all four
/// complete records while the patio overlay is loaded.
extern DirectionWarpEntry gAcropolisPatioWarpEntries[ACROPOLIS_PATIO_WARP_COUNT];

/// Sprite descriptors for mapped patio views 1..19, indexed by mapped view - 1.
///
/// Views 4/17 and 7/13 share source and batch lists. Empty views retain their
/// batch address in `sources.empty`, which supplies no sprite elements.
/// All views have NULL clipping lists. Descriptors and mutable source/batch
/// arrays live with the patio overlay and must outlive packet setup and drawing.
extern SpriteView gAcropolisPatioSpriteViews[ACROPOLIS_PATIO_VIEW_COUNT];

/// Cameras for mapped patio views 1..19, indexed by mapped view - 1.
///
/// Rotations use `ONE` per unit; translations store the negated world origin,
/// and projection distances are pixels. Views 4/17 and 7/13 have equal camera
/// values but remain separate records for their distinct mapped backgrounds.
/// Queued camera tasks borrow a record until application; keep the patio
/// overlay loaded through that call. There is no terminator.
extern ViewCamera gAcropolisPatioViewCameras[ACROPOLIS_PATIO_VIEW_COUNT];

/// Patio collision policies selected by surface class 0..7.
///
/// Classes 0 and 5..7 share a blocking policy with weapon impacts; classes 1
/// and 2 pass probes without impacts, and class 3 blocks without impacts.
/// Class 4 blocks, allows impacts and supplies walk/run/scripted-jump cues.
/// Every class applies grid pushback. Entry 8 is a retained NULL outside the
/// indexed class domain; consumers do not scan it as a terminator. The loaded
/// patio overlay owns the directory and all borrowed policy/sound records.
extern WorldCollisionSurfaceProperties* gAcropolisPatioSurfacePropertiesByClass[ACROPOLIS_PATIO_SURFACE_CLASS_COUNT + 1];

/// Seeds the patio's fourteen fixed light glows and nine nearby moving light points.
///
/// Start in state 0 with a coordinate body and a live EffectWork in
/// `spawnArg2.pointer`. Three larger, four ceiling and seven lower-wall glows
/// use anchor indices 0..13; three points per larger light receive signed
/// XYZ jitter in [-1023, 1024] game units before halfword narrowing.
/// Child placement is copied synchronously. Children borrow the parent
/// coordinate and retain position pointers, so keep the emitter and its work
/// live until the effects end. Later ticks leave this task idle. Requires
/// the patio and gameplay effect resources; failed spawns are not retried.
void acropolisPatioSpawnLightEffectsTask(Task* task);

/// Draws a view-gated, flickering additive sprite for one patio fountain anchor.
///
/// `task` requires a coordinate body and a live `EffectWork` in `spawnArg2.pointer`.
/// Start in state 0. `spawnArg1.value` packs anchor 0..13 in bits 0..3, texture
/// cell 0..2 in bits 8..9, and size in bits 16..27 (zero selects 640). The
/// screen half extent is size * 39 / (SZ3 / 4). Initialization retains only
/// the anchor word and stores size/cell/resting grey in `scale`/`angle`/`period`.
/// Camera views are 1-based. Cancellation or an invisible view suspends the
/// task without freeing it. Requires the patio and gameplay overlays, scratch
/// space and the frame's GPU packet arena; no scratch pointer is retained.
void acropolisPatioFountainJetTask(Task* task);

/// Moves and draws one shimmering grey point of the patio fountain's mist.
///
/// Start in state 0 with a coordinate body, zeroed `EffectWork` in
/// `spawnArg2.pointer`, and an anchor index 0..2 in `spawnArg1.value`. `move`
/// stores signed per-tick velocity; `index` selects drift (0) or gather (1).
/// Gather aims toward the anchor's numeric coordinates in the effect's view
/// parent frame. Drawing uses the composed position cached before this tick's
/// movement, narrows it to s16, and queues an average-blended 1x1 tile at
/// SZ3 / 4 >= `EFFECT_POINT_TILE_MIN_DEPTH`. Invisible views and cancellation
/// suspend motion and drawing without freeing the task. Requires the patio
/// and gameplay overlays, scratch space and the frame's GPU packet arena.
void acropolisPatioFountainMistTask(Task* task);

/// Runs the patio's room-message receiver and initializes its story interactions.
///
/// State 0 registers the receiver and configures actors and scenes for the
/// current story progress; state 1 idles; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisPatioRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_PATIO_H
