#ifndef INCLUDE_ROOMS_DRYFIELD_WATER_HOLE_H
#define INCLUDE_ROOMS_DRYFIELD_WATER_HOLE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_water_hole_801827BC[13];

/// Collision resources for the four daytime water-hole room variants.
///
/// Index by room - 1. Every variant shares the grid, view boundaries and sight
/// occluders; rooms 1/3 use switch-on actions and rooms 2/4 switch-off actions.
/// The linked grid, triggers and occluders stay mutable and must remain loaded
/// until unlinked or replaced by another room's resources.
extern WorldCollisionRoomResources gDryfieldWaterHoleRoomCollisionResources[4];

/// Logical-view maps for the four daytime water-hole room variants.
///
/// Indexed by room - 1: switch on, switch off, alternate switch on, alternate
/// switch off. Flag 0x53 selects the alternate resources for logical views
/// 7 and 8. Each room's reverse search covers views 1..8;
/// the three local maps contain eight bytes, while room 1 borrows
/// `gViewIdentityMap`. Local-map lookups require logical views in 1..8;
/// entries are 1-based camera/image/sprite indices.
/// The directory and local maps remain valid while this room overlay is loaded.
extern u8* gDryfieldWaterHoleRoomViewMaps[4];

/// Logical-view reverse-search lengths for daytime water-hole rooms 1..4.
///
/// Index by room - 1. Each signed-halfword count searches logical slots 1..8;
/// it does not bound forward map lookups or count the camera/sprite resources.
/// The four-element array has no terminator and lives with the room overlay.
extern ViewCount gDryfieldWaterHoleRoomViewCounts[4];

/// Model lighting for the four daytime water-hole room variants.
///
/// Indexed by room - 1. Rooms 1 and 3 use the switch-on light collection and
/// per-view ambient minima; rooms 2 and 4 use the switch-off collection and
/// gameplay's default ambient minima. Borrowed lights remain writable for
/// composition and attenuation; all room pointers expire when the overlay unloads.
extern WorldCoordRoomLighting gDryfieldWaterHoleRoomLighting[4];

/// Three stored daytime water-hole warp endpoints.
///
/// Index by warp - 1. Player and companion share each arrival transform, in room
/// game coordinates with yaw in 4096 units per turn. The driveway endpoint
/// starts in logical view 3 and fades departure; the underpass starts in view 8.
/// The third starts in view 2 and marks GAME_FLAG_MAP_MARK_WATER; its selection
/// is unproven. The first two use the water-hole sound bank. There is no
/// terminator; entries are borrowed while the overlay is loaded.
extern DirectionWarpEntry gDryfieldWaterHoleWarpEntries[3];

/// Twenty-six stored daytime water-hole cameras, indexed by camera resource - 1.
///
/// Logical-view maps select resources 1..20; the remaining six stored cameras
/// are outside those mappings and their selection is unproven. Rotations use
/// Q12, translations store negated room-space camera origins in game units,
/// and projection distances are pixels. No terminator is stored. The map
/// directory borrows this array for the room overlay's loaded lifetime.
extern ViewCamera gDryfieldWaterHoleViewCameras[26];

/// Sprite resources for the twenty mapped daytime water-hole views.
///
/// Index by mapped resource - 1, independently of the eight logical views.
/// Every list keeps decoded background strips and has no clipping areas.
/// Empty source slots retain their batch-list address without selecting any
/// sprites. Sources and batches stay writable; borrowed pointers require the
/// room overlay to remain loaded while packets are initialized or drawn.
extern SpriteView gDryfieldWaterHoleSpriteViews[20];

/// Collision and footstep profiles indexed directly by surface class 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Classes
/// 4 and 6 supply footstep pairs; classes 0, 1, 5 and 7 share the default silent
/// profile, and classes 2 and 3 have distinct silent records. Grid contacts and
/// scripted stair steps supply the class. Pointers expire when this overlay unloads.
extern WorldCollisionSurfaceProperties* gDryfieldWaterHoleSurfacePropertiesByClass[8];

/// Advances and draws one expanding, fading water-surface ripple in this room.
///
/// `task` must be a live counted effect with one coordinate body and an owned,
/// zero-initialized `EffectWork` in `spawnArg2.pointer`, initially in state 0.
/// `spawnArg1` bits 0..11 give the initial local half-side in game coordinate
/// units (0..4095); higher bits are ignored. The room overlay must remain loaded
/// throughout the effect's lifetime.
///
/// Each running update grows the half-side by 32 and dims RGB brightness by 2,
/// drawing 32 times from brightness 64 down to 2. The first update seeds a
/// random yaw at 4096 units per turn; it is composed on the next running update.
/// Non-running controls redraw the retained values without advancing them.
/// Cancellation draws once before teardown; cancellation and fade completion
/// free the work, decrement the live-effect count and release the task/body.
void dryfieldWaterHoleWaterRippleTask(Task* task);

/// Advances and draws one eight-cell water-spray particle for this room.
///
/// `task` must be a live counted effect with owned `EffectWork` in
/// `spawnArg2.pointer`, a coordinate body, initial state 0 and cell index 0.
/// `effectSpawn` supplies this setup for `EFFECT_DRYFIELD_WATER_HOLE_WATER_SPRAY`.
/// The room overlay and water textures must remain loaded while the task lives.
/// Translation and velocity use the coordinate's parent space, normally view
/// space; drawing requires initialized scratch storage and frame primitive space.
///
/// `spawnArg1` bits 0..11 give perspective size (0..4095), bits 12..15 give
/// running updates per cell (0 selects 1), and bits 16..23 give launch speed
/// in coordinate units per update (0 selects 64). Bits 24..27 select the
/// launch direction: 0 stationary, 1 upward burst, 2 all-axis spray, 3 narrow
/// upward jet, 5 copied spawn-offset direction; other values leave it zero.
/// Generated directions are normalized in Q12 and scaled into signed halfword
/// velocities. A supplied nonzero `EffectWork::move` bypasses generation and
/// scaling and enables movement. Nonzero bits 28..31 select the upright
/// eight-cell grid; otherwise the eight-cell strip uses a fixed random angle
/// in 4096 units per turn. Both drawers receive an unsigned 16-bit cell index.
///
/// The first running update initializes without drawing or moving. Later
/// updates draw, move and add 6 to Y velocity, retaining the low 16 bits;
/// stationary particles skip movement and gravity. Cells 0..7 each last the
/// decoded period, so a fresh particle retires after 1 + 8 * period running
/// updates. Suspended updates redraw cached state without composing or aging;
/// cancellation retires without drawing. Retirement frees the work, decrements
/// the effect count and tears down the task and coordinate body. Retained
/// pointers to these allocations expire at retirement.
void dryfieldWaterHoleWaterDriftTaskU16(Task* task);

/// Emits player water splashes and draws the water hole's switch-enabled beams.
///
/// Start in state 0 with a live player model containing coordinates 14 and 17,
/// a coordinate body and `EffectWork` in `spawnArg2.pointer`. Initialization selects
/// this room's ripple/spray IDs and snapshots two composed part positions.
/// State 1 emits only while effect control is running and session waterY is
/// less than the player's local root Y. Each part rolls a ripple then a spray
/// at waterY, using Manhattan movement as odds out of 512, plus 32 for ripples.
/// Odds and position samples narrow to signed halfwords without clamping;
/// paused or out-of-water updates leave history unchanged. Spawned children
/// copy the temporary coordinate and live independently.
///
/// Underpass switch 1 also enables beam pairs in views 3/4, 4/6 and 7,
/// independently of the splash gates. Requires a current view in 1..8, loaded
/// room effect resources, composed coordinates, scratch and frame packet space.
/// This controller does not release itself; room-effect teardown owns its lifetime.
void dryfieldWaterHoleRoomEffectsTask(Task* task);

/// Runs the daytime water-hole room task for one update.
///
/// State 0 registers the message receiver and starts the water surface, 1
/// idles and 2 releases the task. The state index is unchecked. Requires a
/// live task and the room overlay loaded throughout its lifetime.
void dryfieldWaterHoleRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WATER_HOLE_H
