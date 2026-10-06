#ifndef INCLUDE_ROOMS_SHELTER_B2_MAIN_CORRIDOR_H
#define INCLUDE_ROOMS_SHELTER_B2_MAIN_CORRIDOR_H

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

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b2_main_corridor_80182EEC[4];

extern SVECTOR D_shelter_b2_main_corridor_80182EFC[12];

/// Undisplaced water-surface Y in signed world units, shared with the room's actors.
extern s16 gShelterB2MainCorridorWaterY;

extern AreaVariant D_shelter_b2_main_corridor_8018933C[22];

// shelter_b2_main_corridor
extern u8* D_shelter_b2_main_corridor_801830CC[];

extern ViewCount D_shelter_b2_main_corridor_801830D0[];

extern DirectionWarpEntry D_shelter_b2_main_corridor_801830D4[];

extern WorldCollisionGrid D_shelter_b2_main_corridor_80184440;

extern ViewCamera D_shelter_b2_main_corridor_80184464[];

extern SpriteView D_shelter_b2_main_corridor_80188848[];

extern WorldCoordRoomLights D_shelter_b2_main_corridor_80188BE4;

extern WorldCollisionTrigger D_shelter_b2_main_corridor_80188BFC[];

extern WorldCollisionTrigger D_shelter_b2_main_corridor_801893EC[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_main_corridor_80189624[];

/// Runs the corridor's message receiver and starts its water-surface task.
///
/// Start a bodyless task in state 0. The first tick installs the borrowed room
/// message table, registers `GAME_TASK_SLOT_ROOM`, and spawns the water task.
/// State 1 idles; state 2 tears down the task. Other states are invalid table
/// indices. Keep the corridor overlay loaded while the task or its messages run.
void shelterB2MainCorridorRoomTask(Task* task);

/// Advances and draws one expanding, fading water-surface ripple in this room.
///
/// Requires a counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state 0. Bits 0..11 of `spawnArg1` supply the
/// initial local half-side in world units (0..4095); higher bits are ignored.
/// Running ticks grow it by 32 and fade brightness from 64 by 2, lasting 32
/// running ticks. The first tick chooses a random yaw after composing the
/// coordinate. Non-running ticks redraw without aging; cancellation redraws
/// once before freeing the work, decrementing the effect count and destroying
/// the task and body. Keep this overlay and the effect controller live.
void shelterB2MainCorridorWaterRippleTask(Task* task);

/// Advances and draws one eight-cell water-spray particle in the corridor.
///
/// Requires a live counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// one coordinate body, initial state 0 and cell index 0. Draws the coordinate's
/// existing `workm`; local motion dirties its cache without composing it.
/// The effect spawner initially composes the coordinate beneath the view.
///
/// `spawnArg1` bits 0..11 give size scale (0..4095), bits 12..15 the running
/// ticks per cell (0 selects 1), bits 16..23 speed in parent-coordinate units
/// per tick (0 selects 64), and bits 24..27 the velocity kind: 0 stationary,
/// 1 upward burst, 2 all-axis spray, 3 narrow upward jet, 5 direction from
/// the copied spawn offset. Other kinds retain the zero direction before
/// normalization. Nonzero bits 28..31 select an upright tile; otherwise the
/// quad retains a random angle in 4096 units per turn. A preloaded nonzero
/// `move` bypasses generation and scaling; `step` becomes 64 to enable motion.
///
/// The first running tick initializes without drawing or moving. Later ticks
/// draw the current cell, advance local translation by signed halfword velocity
/// and add 6 to its Y component, retaining the low 16 bits. Stationary particles
/// skip motion and gravity. Cells 0..7 last `period` ticks each; retirement
/// follows 8 * `period` running ticks after initialization (8..120 ticks).
///
/// Non-running control below cancellation redraws without initialization or
/// aging; cancellation retires without drawing. Retirement frees the work,
/// decrements the effect count and destroys the task and coordinate body.
/// Keep this overlay, controller, cached matrix and drawing resources live;
/// queued primitives must remain available until GPU consumption.
void shelterB2MainCorridorWaterDriftTask(Task* task);

/// Runs a pink charge flash, a screen tint at its peak, and a fading star.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`. Start in state 0 with a positive charge duration in
/// `spawnArg1.value`, in running ticks; it becomes a countdown. Nonzero room
/// effect control pauses updates, and values at least 4 cancel. Completion or
/// state 3 releases the work and task. Keep this overlay and controller live.
void shelterB2MainCorridorRoomVisualEffectsFlashTask(Task* task);

/// Records and draws two fading trails between parent-relative endpoints.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`; its borrowed parent coordinate must remain live.
/// State 0 allocates two eight-slot histories in `Task::work`, seeding them at
/// offsets (0, 190, -15) and (0, 1085, 180) in parent-space world units.
/// Allocation failure resets age and retries. State 1 stores world-space
/// snapshots and draws seven quads with RGB weights 1:2:3. `spawnArg1.value`
/// is an exact expiry age, compared only in state 1; zero never expires, and
/// age is signed 16-bit. Expiry releases both histories, the counted work and
/// the task. Control values below 2 allow updates, including pause value 1;
/// values at least 2 freeze it. Keep this overlay and effect controller live.
void shelterB2MainCorridorRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_b2_main_corridor_80181C98(Task* task);

/// Selects the corridor's effects and draws beams and flares for the mapped view.
///
/// Start in state 0 to bind the room's flash, twin-trail, spark-burst, ripple
/// and spray IDs; state 1 draws on that same tick and all later ticks. Mapped
/// view's low byte selects views 2..11; other views draw nothing. View 6 also
/// draws view 7's beams. Drawing ignores room effect pause/cancel control and
/// leaves the task live. Requires this loaded overlay, current view matrices,
/// scratch stack, packet arena and ordering table; queued packets must remain
/// live until GPU consumption. At most 24 beam quads, two flares and their
/// blend commands are queued per tick. The task's coordinate body is unused.
void shelterB2MainCorridorDrawViewGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_MAIN_CORRIDOR_H
