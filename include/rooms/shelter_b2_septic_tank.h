#ifndef INCLUDE_ROOMS_SHELTER_B2_SEPTIC_TANK_H
#define INCLUDE_ROOMS_SHELTER_B2_SEPTIC_TANK_H

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
extern SVECTOR* D_shelter_b2_septic_tank_801836A4[4];

extern SVECTOR D_shelter_b2_septic_tank_801836B4[12];

/// Undisplaced water-surface Y in signed world units, shared with the room's actors.
extern s16 gShelterB2SepticTankWaterY;

extern AreaVariant D_shelter_b2_septic_tank_80186F40[22];

// shelter_b2_septic_tank
extern u8* D_shelter_b2_septic_tank_8018356C[];

extern ViewCount D_shelter_b2_septic_tank_80183570[];

extern DirectionWarpEntry D_shelter_b2_septic_tank_80183574[];

extern WorldCollisionGrid D_shelter_b2_septic_tank_80183E0C;

extern ViewCamera D_shelter_b2_septic_tank_80183E30[];

extern SpriteView D_shelter_b2_septic_tank_801866F4[];

extern WorldCoordRoomLights D_shelter_b2_septic_tank_80186A3C;

extern WorldCollisionTrigger D_shelter_b2_septic_tank_80186A54[];

extern WorldCollisionTrigger D_shelter_b2_septic_tank_80186C1C[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_septic_tank_80187014[];

/// Runs room initialization, encounter monitoring or teardown for the septic tank.
///
/// Requires a live bodyless room task with state 0 (register messages and spawn
/// water), 1 (monitor the introductory encounter) or 2 (destroy the task).
/// Keep the room overlay loaded while dispatching; state 0 publishes the task
/// in the room slot for synchronous messages.
void shelterB2SepticTankRoomTask(Task* task);

/// Selects the room's effects and draws beams and discs for the mapped view.
///
/// State 0 installs the flash, twin-trail, spark-burst, ripple and spray IDs,
/// then draws immediately as state 1. Mapped views 2..6 select world-space
/// point lists; other views emit nothing. Requires the active room and view
/// transforms, ordering table and primitive arena. Spawn arguments are unused.
void shelterB2SepticTankGlowTask(Task* task);

/// Advances and draws an expanding, fading water-surface ripple.
///
/// Start in state 0 with a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`. The low 12 bits of `spawnArg1` give the initial local
/// half-side in world units. Running updates grow it by 32 and dim brightness
/// from 64 by 2, retiring after 32 draws. Other control values redraw without
/// advancing; cancellation draws once before releasing work and task. Keep
/// the room overlay and effect controller live through retirement.
void shelterB2SepticTankWaterRippleTask(Task* task);

/// Advances and draws one eight-cell water-spray particle.
///
/// Start in state 0 with a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`, with frame index 0. `spawnArg1` packs size scale
/// (bits 0..11), updates per cell (12..15, zero means 1), speed (16..23, zero
/// means 64), velocity kind
/// (24..27: 0 stationary, 1 upward burst, 2 spray, 3 narrow jet, 5 from `pos`)
/// and upright drawing (any bits 28..31). A supplied nonzero velocity bypasses
/// launch generation. After initialization, each draw moves in parent-space
/// world units and adds 6 to a moving particle's Y velocity.
/// Paused particles redraw; cancellation or animation completion releases work
/// and task. Keep the room overlay, parent and effect controller live.
void shelterB2SepticTankWaterDriftTask(Task* task);

/// Runs a pink charge flash, a screen tint at its peak and a fading star.
///
/// Start in state 0 with a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`; `spawnArg1.value` is a positive charge duration in
/// running ticks, consumed as a countdown. Nonzero room effect control pauses;
/// values at least 4 cancel. Completion or state 3 releases work and task.
/// Keep the room overlay and effect controller live.
void shelterB2SepticTankRoomVisualEffectsFlashTask(Task* task);

/// Records and draws two fading trails between parent-relative endpoints.
///
/// Start in state 0 with a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`; its borrowed parent must remain live. Allocates two
/// eight-slot histories in `Task::work`, retrying allocation failure, then
/// records world-space snapshots and draws seven quads with RGB weights 1:2:3.
/// `spawnArg1.value` is an exact expiry age tested in state 1; zero never
/// expires and age is signed 16-bit. Expiry releases histories, work and task.
/// Control below 2 allows updates, including pause value 1; higher values
/// freeze it. Keep the room overlay and effect controller live.
void shelterB2SepticTankRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke puffs or orange rings and bouncing sparks.
///
/// Start in state 0 with a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`, with age zero. Nonzero `spawnArg1.value` selects smoke;
/// zero selects rings and sparks. Enters release at active age seven and frees
/// work and task on the next active tick. Nonzero room effect control below
/// four pauses; four or above cancels. Children run independently. Keep the
/// room overlay, effect controller and borrowed parent live through retirement.
void shelterB2SepticTankRoomVisualEffectsSparkBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_SEPTIC_TANK_H
