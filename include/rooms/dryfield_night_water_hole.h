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

/// Lighting and logical-view ambient minima for the night water hole's four rooms.
///
/// Indexed by room - 1 (rooms 1..4). Rooms 1 and 3 share the underpass-switch-1
/// on bank; rooms 2 and 4 share its off bank. Ambient entries use logical views
/// 1..11, independently of the mapped camera/image index. The stage directory
/// borrows these records and their mutable lights while this overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightWaterHoleRoomLighting[4];

/// Logical-view maps for the night water hole's four room selectors.
///
/// Indexed by room - 1, then logical view - 1 (rooms 1..4, views 1..11).
/// Entries are unsigned 1-based camera/image/sprite indices within 1..26;
/// each map retains a twelfth zero byte outside the view count, not a terminator.
/// Rooms 1/3 select switch-on resources, rooms 2/4 switch-off resources; rooms
/// 3/4 substitute mapped indices for logical views 7/8. The stage directory
/// borrows the maps only while this overlay is loaded.
extern u8* gDryfieldNightWaterHoleViewMaps[4];

extern ViewCount D_dryfield_night_water_hole_80180AA4[];

extern DirectionWarpEntry D_dryfield_night_water_hole_80180AAC[];

extern ViewCamera D_dryfield_night_water_hole_80180F74[];

extern SpriteView D_dryfield_night_water_hole_80182384[];

/// Active collision policies and footstep cues for this room's eight surface classes.
///
/// Indexed directly by surface class 0..7, independently of room selector.
/// All defaults block probes, allow weapon impacts and apply pushback. Classes
/// 1/2 use footstep bases 37/39/41; class 6 uses 61/63/61. Classes 0/3/4/5/7
/// have no footstep cues. Opening the Shelter route replaces slots 1/2/4 with
/// records using 61/63/61. Cue order is walk/run/scripted jump in the loaded
/// type-1 sound bank. The stage directory borrows this mutable table and its
/// records only while this overlay is loaded; there is no terminal entry.
extern WorldCollisionSurfaceProperties* gDryfieldNightWaterHoleSurfacePropertiesByClass[8];

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

/// Emits movement-driven player water splashes and draws the switched-on light shafts.
///
/// Requires state zero, counted `EffectWork` in `spawnArg2.pointer` and the live
/// player's model coordinates 14 and 17. The room owns one two-position history;
/// concurrent emitters would overwrite it. Initialization samples cached composed XYZ
/// into signed halfwords. While the Shelter route is closed, room effects run
/// and the player root is below the water surface (positive Y downward), each
/// part's summed XYZ movement sets halfword odds out of 512. Ripple odds add 32;
/// spray odds omit that bias. Two LCG draws occur per part even if spawning
/// fails. Inactive frames retain the previous samples. Each child draws from
/// its copied surface coordinate; this task does not retain the temporary.
/// Underpass switch 1 also enables view-selected shafts. Requires view 0..31
/// and loaded room resources throughout; external teardown owns retirement.
void dryfieldNightWaterHoleSplashAndLightShaftsTask(Task* task);

/// Runs the night water hole's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers, restores progress-dependent scenery
/// and cues arrival or ending actors when required; state 1 waits for
/// messages; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightWaterHoleRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_HOLE_H
