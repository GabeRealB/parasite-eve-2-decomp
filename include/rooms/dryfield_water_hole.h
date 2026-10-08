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

// dryfield_water_hole
extern WorldCollisionRoomResources D_dryfield_water_hole_8017FD2C[];

extern u8* D_dryfield_water_hole_8017FD84[];

extern ViewCount D_dryfield_water_hole_8017FD94[];

extern WorldCoordRoomLighting D_dryfield_water_hole_8017FD9C[];

extern DirectionWarpEntry D_dryfield_water_hole_8017FDBC[];

extern ViewCamera D_dryfield_water_hole_80180284[];

extern SpriteView D_dryfield_water_hole_80181634[];

extern WorldCollisionSurfaceProperties* D_dryfield_water_hole_801828AC[];

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
