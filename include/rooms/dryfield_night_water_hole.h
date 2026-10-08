#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_HOLE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_HOLE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaApplyRec D_dryfield_night_water_hole_80183618[4];

extern AreaVariant D_dryfield_night_water_hole_80183418[22];

// dryfield_night_water_hole
extern WorldCollisionRoomResources D_dryfield_night_water_hole_80180A04[];

extern WorldCoordRoomLighting D_dryfield_night_water_hole_80180A44[];

extern u8* D_dryfield_night_water_hole_80180A94[];

extern ViewCount D_dryfield_night_water_hole_80180AA4[];

extern DirectionWarpEntry D_dryfield_night_water_hole_80180AAC[];

extern ViewCamera D_dryfield_night_water_hole_80180F74[];

extern SpriteView D_dryfield_night_water_hole_80182384[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_water_hole_801835F8[];

/// Advances and draws one eight-cell water-spray particle in this room.
///
/// Requires a counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, initial state zero and cell index zero. `spawnArg1`
/// bits 0..11 give perspective size (0..4095), bits 12..15 running updates per
/// cell (0 selects 1), and bits 16..23 launch speed in parent-coordinate units
/// per update (0 selects 64). Bits 24..27 select velocity: 0 stationary,
/// 1 upward burst, 2 all-axis spray, 3 narrow upward jet, 5 copied spawn-offset
/// direction; other values normalize the zero direction. Nonzero bits 28..31
/// select upright drawing; otherwise a random rotation is retained, in 4096
/// units per turn. A supplied nonzero `EffectWork::move` bypasses generation
/// and scaling.
///
/// The first running update initializes without drawing or moving. Later
/// updates draw, move and add 6 to Y velocity, retaining its low 16 bits;
/// stationary particles skip movement and gravity. Cells 0..7 each last the
/// decoded period. Suspended updates redraw without aging or composing;
/// cancellation retires without drawing. Drawing requires initialized scratch
/// and frame primitive space. Retirement frees the work and tears down the
/// task and body, decrementing the effect count; retained pointers expire.
void dryfieldNightWaterHoleWaterDriftTaskU16(Task* task);

/// Advances and draws one expanding, fading water-surface ripple in this room.
///
/// Requires a counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state zero. `spawnArg1` bits 0..11 give the
/// initial local half-side in game-coordinate units (0..4095); higher bits are
/// ignored. Running updates compose the coordinate, grow the half-side by 32,
/// draw, then fade brightness from 64 by 2; a fresh ripple lasts 32 running
/// updates. The first update selects a random surface yaw after composing,
/// so the new rotation first appears on the next running update.
///
/// Non-running updates redraw without aging or composing; cancellation redraws
/// once before retirement. Drawing requires initialized scratch and frame
/// primitive space. Retirement frees the work and tears down the task and body,
/// decrementing the effect count; retained pointers expire.
void dryfieldNightWaterHoleWaterRippleTask(Task* task);

void func_dryfield_night_water_hole_8017E6D0(Task* arg0);

/// Runs the night water hole's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers, restores progress-dependent scenery
/// and cues arrival or ending actors when required; state 1 waits for
/// messages; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightWaterHoleRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_HOLE_H
