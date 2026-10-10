#ifndef INCLUDE_ROOMS_ACROPOLIS_PROMENADE_H
#define INCLUDE_ROOMS_ACROPOLIS_PROMENADE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Promenade placement/resource pairs indexed directly by placement variant.
///
/// Slots 1, 2, 3, 7 and 8 are populated; 0, 4..6 and 9..16 are empty.
/// This seventeen-element table has no terminator. Gameplay's Acropolis area
/// 11 record borrows it, and the populated map placements and room resource
/// lists, while their overlays are loaded. Loading and spawning perform no
/// variant bounds check; the selected index must be in 0..16.
extern AreaVariant gAcropolisPromenadeAreaVariants[17];

/// Collision resources for promenade rooms 1 and 2, indexed by room minus one.
///
/// Each room selects its own grid and borrows the same six view boundaries and
/// six action triggers; neither has sight occluders. Setup binds the mutable
/// resources to the active view. Keep the promenade overlay loaded until its
/// grid is replaced and its triggers are unlinked. This table has no terminator.
extern WorldCollisionRoomResources gAcropolisPromenadeRoomCollisionResources[2];

/// Logical-view maps for promenade rooms 1 and 2, indexed by room minus one.
///
/// Each room has logical views 1..13; index at view minus one to obtain the
/// 1-based camera, image and sprite index. Room 1 uses `gViewIdentityMap`;
/// room 2 exchanges mapped views 4/8 and 5/9. The map overlay borrows these
/// pointers while the promenade overlay is loaded; there is no terminator.
extern u8* gAcropolisPromenadeViewMaps[2];

/// Logical-view counts for promenade rooms 1 and 2, indexed by room minus one.
///
/// Both rooms search thirteen bytes of their logical-view maps. These signed
/// element counts have no terminator and require the promenade overlay loaded;
/// they describe the reverse-lookup domain, independently of camera capacity.
extern ViewCount gAcropolisPromenadeViewCounts[2];

/// Lighting descriptors for promenade rooms 1 and 2, indexed by room minus one.
///
/// Both borrow the same mutable point-light collection and use the default
/// ambient minima. The descriptors and lights require the promenade overlay
/// to stay loaded; this two-element table has no terminator.
extern WorldCoordRoomLighting gAcropolisPromenadeRoomLightingTable[2];

/// Five promenade arrival/departure endpoints, indexed by warp ID minus one.
///
/// Player and companion share each placement. Positions use game units and yaw
/// uses 4096 units per turn; initial views are room-local logical view IDs.
/// Endpoints 2, 3 and 5 carry paired arrival/departure sound cues, and 2 and 5
/// carry map-marker flags. The stage map borrows this unterminated table while
/// the promenade overlay is loaded; valid warp IDs are 1..5.
extern DirectionWarpEntry gAcropolisPromenadeWarpEntries[5];

/// Sprite resources for mapped promenade views 1..13, indexed by view minus one.
///
/// Views 2..9 have source sprites; 1 and 10..13 have empty batch lists. Every
/// view enables decoded image strips with a zero-count first batch, and none
/// supplies clipping areas. Views 1 and 13 share their empty list. The stage
/// map borrows these descriptors and their mutable lists while the promenade
/// overlay is loaded. The descriptor array has no terminator.
extern SpriteView gAcropolisPromenadeSpriteViews[13];

/// Cameras for mapped promenade views 1..13, indexed by camera index minus one.
///
/// Rotation coefficients have twelve fractional bits; translation stores the
/// negated camera origin in game units. Projection distances are pixels: 499
/// for view 1, 230 for views 2..12 and 257 for view 13. Room 2's logical-view map
/// permutes camera selection. The map borrows this unterminated array while the
/// promenade overlay is loaded; queued camera application requires it to stay
/// readable until the transform is copied.
extern ViewCamera gAcropolisPromenadeViewCameras[13];

/// Promenade collision and footstep policies, indexed by surface class 0..7.
///
/// Classes 0, 5 and 6 share blocking, pushback-enabled properties without
/// footstep cues. Classes 1..4 also provide footstep cue bases. Class 7 passes
/// probes and suppresses pushback while allowing weapon impacts. Classes 3 and
/// 7 supply cue bases for entries absent from this room's loaded sound bank.
/// The map overlay borrows all eight pointers and their records while the
/// promenade overlay is loaded; there is no terminator.
extern WorldCollisionSurfaceProperties* gAcropolisPromenadeSurfacePropertiesByClass[8];

/// Emits the promenade's view-dependent lamps, ground glow and screen drips.
///
/// Requires a counted effect with a single-coordinate body and zeroed
/// `EffectWork` in `spawnArg2.pointer`. Its exit callback owns that allocation.
/// Cancellation stops all updates. Otherwise the signed-halfword age
/// advances each tick, including suppressed mapped view 7. Valid mapped views
/// are 1..13; masks select twelve room-local lamp placements and the view-6
/// glow group. `scale` remembers the previous mapped view: entering a drip
/// view seeds forty drips, and staying there emits two per tick. Keep the work
/// and room resources live until effect teardown.
void acropolisPromenadeRoomEffectTask(Task* task);

/// Draws one frame of an animated lamp core and a rotating, flickering flare.
///
/// Requires a live counted effect with a single-coordinate body and owned
/// `EffectWork` in `spawnArg2.pointer`. The signed low halfword of `spawnArg1`
/// is the phase: its signed remainder modulo six selects the 16-texel core cell;
/// added to `gDisplayState.animFrame`, it gives the flare angle in 4096 units
/// per turn, narrowed to a signed halfword. Projects the composed centre through
/// the current view; camera Z / 4 below 17 queues neither quad. Requires initialized
/// GTE, 24 scratch-stack bytes and space for two `POLY_FT4` packets in the current
/// frame arena. Even a clipped draw consumes one packet. Always releases work
/// and task before returning; queued packets remain live until GPU completion.
void acropolisPromenadeGlowStarTask(Task* task);

/// Smears a one-pixel framebuffer row down each frame to draw one screen drip.
///
/// Requires a live counted effect with owned, initially zeroed `EffectWork` in
/// `spawnArg2.pointer`; `spawnArg1.value` is the mapped camera view to retain.
/// Seeds X in 0..239, initial Y in 176..239, lifetime in 30..119 frames,
/// width in 16..79 pixels and fall speed in 1..4 frames per row. `move` holds
/// the initial pixel position, `scale` the lifetime, `angle` the width and
/// `period` the frames per row. Copies only source rows below 239 within the
/// current draw buffer, into a row-derived ordering-table slot (41..80).
/// Requires one `DR_MOVE` packet per visible frame; packets live until GPU
/// completion. Releases work and task on a view change, expiry or bottom edge.
void acropolisPromenadeScreenDripTask(Task* task);

/// Draws a flickering additive square in the coordinate body's local XZ plane.
///
/// Requires a live counted effect with a single-coordinate body and owned
/// `EffectWork` in `spawnArg2.pointer`. Composes the coordinate and transforms
/// the 768-unit half-side through its world matrix, narrowing corners to s16.
/// Copies the spawn phase to `age`, but it does not affect this one-frame draw.
/// Uses the 40-by-40 texture cell at (0, 16), with random grey in 0..15; draws
/// when the last corner's SZ3 / 4 plus 32 is at least 17. Requires initialized
/// GTE, 36 scratch-stack bytes and one `POLY_FT4` in the current frame arena,
/// consumed even when clipped. Releases scratch, work and task on return;
/// queued packets remain live until GPU completion.
void acropolisPromenadeGroundGlowTask(Task* task);

/// Draws one frame of a flickering additive lamp billboard and releases the effect.
///
/// Requires a live counted effect with a single-coordinate body and owned work
/// in `spawnArg2.pointer`. The full signed `spawnArg1.value` must be 0..2:
/// it selects a 40-by-40 texture cell and palette, with grey levels 32/40,
/// 96/112 or 32/44 on even/odd animation frames. Composes the coordinate and
/// narrows its composed origin to s16 before projection through `GsWSMATRIX`.
/// The composed transform includes the view parent. At camera Z / 4
/// >=17, queues a square of pixel half-side 24960 / depth; GTE flags do not
/// gate drawing. Requires initialized GTE, 20 scratch-stack bytes and space
/// for one `POLY_FT4` packet, consumed even when clipped. Always releases
/// scratch, work and task; queued packets remain live until GPU completion.
void acropolisPromenadeGlowLampTask(Task* task);

/// Runs the promenade's room messages, arrival scene and battle follow-up.
///
/// State 0 registers the receiver and spawns the bridge-plane model; state 1
/// checks arrival and battle progress; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisPromenadeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_PROMENADE_H
