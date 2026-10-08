#ifndef INCLUDE_ROOMS_SHELTER_B4_WATER_SUPPLY_H
#define INCLUDE_ROOMS_SHELTER_B4_WATER_SUPPLY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern u8 D_shelter_b4_water_supply_801826FC[64];

extern AreaVariant D_shelter_b4_water_supply_80184CA4[12];

// shelter_b4_water_supply
extern u8* D_shelter_b4_water_supply_8018273C[];

extern ViewCount D_shelter_b4_water_supply_80182740[];

extern DirectionWarpEntry D_shelter_b4_water_supply_80182744[];

extern WorldCollisionGrid D_shelter_b4_water_supply_80182E3C;

extern ViewCamera D_shelter_b4_water_supply_80182E60[];

extern SpriteView D_shelter_b4_water_supply_80183F90[];

extern WorldCoordRoomLights D_shelter_b4_water_supply_801843D4;

extern WorldCollisionTrigger D_shelter_b4_water_supply_801843EC[];

extern WorldCollisionTrigger D_shelter_b4_water_supply_80184944[];

extern WorldCollisionOccluder D_shelter_b4_water_supply_80184D04[];

extern WorldCoordRoomAmbientEntry D_shelter_b4_water_supply_80184D7C[];

extern WorldCollisionSurfaceProperties* D_shelter_b4_water_supply_80184E14[];

/// Runs the water-supply room receiver's initialize, idle or teardown state.
///
/// Requires a live bodyless task with state 0..2 and this package loaded for
/// its lifetime. State 0 registers the receiver and starts water rendering;
/// state 1 waits for messages; state 2 releases the task.
void shelterB4WaterSupplyRoomTask(Task* task);

/// Installs this room's effect IDs, emits player-water splashes and draws light beams.
///
/// Requires a live player model with coordinates through 17 and a writable
/// `EffectWork` in `spawnArg2.pointer`. State zero records parts 14 and 17's
/// composed XYZ without refreshing them; later ticks sample movement only while
/// room effect control is running and the water Y is less than the player's root Y.
/// Samples and odds narrow to signed halfwords. Two ordered rolls per part use
/// odds out of 512, with a 32-point ripple bias; no clamp occurs. Pauses retain
/// old samples. Spawns snapshot the temporary surface coordinate; the children
/// never read its retained source pointer. Beams draw for
/// mapped views 2..11 irrespective of effect control. The task lives until
/// external teardown; keep the player, room resources and render state live.
void shelterB4WaterSupplySplashAndLightBeamsTask(Task* task);

/// Runs the water-supply room's expanding, fading water-surface ripple.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state zero. `spawnArg1` bits 0..11 give the
/// initial local half-side in coordinate units; higher bits are ignored.
/// Running updates grow by 32 and dim by 2, lasting 32 updates. Non-running
/// control redraws the retained state; cancellation redraws once before
/// releasing the work and task. The coordinate's parent must remain live.
void shelterB4WaterSupplyWaterRippleTask(Task* task);

/// Runs the water-supply room's eight-cell water-spray particle with gravity.
///
/// Requires a counted effect task with owned zeroed `EffectWork` in
/// `spawnArg2.pointer`, a coordinate body, initial state and frame index zero.
/// `spawnArg1` bits 0..11 give perspective size scale, bits 12..15 updates per
/// cell (0 selects 1), and bits 16..23 parent-space speed (0 selects 64).
/// Bits 24..27 select velocity: 0 stationary, 1 upward burst, 2 all-axis spray,
/// 3 narrow upward jet, 5 the copied offset in `pos`; other kinds retain a zero
/// direction before normalization. Any set bits 28..31 select upright drawing;
/// otherwise drawing is rotated. A preseeded nonzero `move` bypasses generation.
/// Initialization composes without drawing; later updates draw, move and add
/// 6 to signed-halfword Y velocity. Non-running control redraws without aging;
/// cancellation releases the work and task without drawing.
void shelterB4WaterSupplyWaterDriftTask(Task* task);

/// Runs this room's attached charge disc, player-joint sparks and fading release ring.
///
/// Requires a coordinate body and counted, owned, zeroed `EffectWork` in
/// `spawnArg2.pointer`. Its borrowed parent and copied offset establish the
/// attachment; `spawnArg1.value` selects tint 0 or 1. Start in state zero.
/// The owner requests flicker, release or cancellation through the shared
/// glow-disc states. Growing emits an adopted flying spark every fourth age
/// from player parts 3..18, requiring that live model and installed callback.
/// Nonzero room effect control pauses below four and cancels at four or above;
/// release fades the ring and frees work and child tasks. Keep this package and
/// coordinate ancestors live until teardown, then discard retained pointers.
void shelterB4WaterSupplyRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs the water-supply glowing-disc spark toward an initial target coordinate.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and state zero. `spawnArg1.pointer` borrows a `GfxCoord`
/// through the first running update; both world matrices must be composed.
/// That update fixes a parent-space step at 204/4096 of the initial separation.
/// Later updates move and draw on odd ages, releasing the work and task at age
/// 20. Non-running control pauses without drawing; cancellation releases the
/// effect. The target is not sampled after initialization.
void shelterB4WaterSupplyRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the water-supply orange burst with a growing disc, glow and fading ring.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state zero; `spawnArg1` is ignored. Running
/// updates compose, grow the glow and fade the ring before the central disc.
/// Non-running control pauses without drawing. Cancellation or completed fading
/// releases the work and task; callers must not retain released pointers.
void shelterB4WaterSupplyRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B4_WATER_SUPPLY_H
