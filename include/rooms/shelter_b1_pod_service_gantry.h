#ifndef INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H
#define INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"

extern AreaVariant D_shelter_b1_pod_service_gantry_8017FBF8[11];

// shelter_b1_pod_service_gantry
extern u8* D_shelter_b1_pod_service_gantry_8017FB1C[];

extern ViewCount D_shelter_b1_pod_service_gantry_8017FB20[];

extern DirectionWarpEntry D_shelter_b1_pod_service_gantry_8017FB24[];

extern WorldCollisionGrid D_shelter_b1_pod_service_gantry_801801C4;

extern ViewCamera D_shelter_b1_pod_service_gantry_801801E8[];

extern SpriteView D_shelter_b1_pod_service_gantry_80181BA0[];

extern WorldCoordRoomLights D_shelter_b1_pod_service_gantry_801824F4;

extern WorldCollisionSurfaceProperties* D_shelter_b1_pod_service_gantry_80182520[];

void func_shelter_b1_pod_service_gantry_8017D89C(Task* task);

void func_shelter_b1_pod_service_gantry_8017F450(GfxCoord* arg0, s32 arg1, s32 arg2, s16 arg3);

/// Seeds the chain-glow flicker offsets and selects the room's water-spray effect.
///
/// A live task in state 0 advances the shared random sequence eight times,
/// storing bits 16..23 as one byte per chain, then enters idle state 1.
/// Any nonzero state does nothing. Spawn arguments and the coordinate body are
/// unused; this callback allocates no work and leaves teardown to its owner.
void shelterB1PodServiceGantryInitEffectsTask(Task* task);

/// Advances and draws one counted eight-cell water-spray particle for this room.
///
/// Requires a live coordinate-body task in state 0 with owned, initialized
/// `EffectWork` in `spawnArg2.pointer`, initially at cell 0, and a live
/// `gRoomEffectState`. Translation and velocity use the coordinate's parent
/// space, normally view space. Drawing requires initialized scratch storage
/// and capacity for one textured-quad packet.
///
/// `spawnArg1.value` packs perspective size in bits 0..11, running updates per
/// cell in 12..15 (0 selects 1), launch speed in 16..23 (0 selects 64), and
/// velocity kind in 24..27 (0 stationary, 1 upward burst, 2 all-axis spray,
/// 3 narrow upward jet, 5 the copied `pos` direction; others launch at zero).
/// Any bits 28..31 select an upright sprite; otherwise the random spin angle
/// stays fixed, in 4096 units per turn. A supplied nonzero `move` bypasses
/// direction generation and scaling. Velocity is in parent-coordinate units
/// per running update and its components retain signed halfword precision.
///
/// The first running update initializes without drawing or moving. Subsequent
/// updates draw before moving, add 6 to moving particles' Y velocity, and
/// retire after eight cells of the selected period. Suspended control values
/// below 4 redraw without aging; control >= 4 retires without drawing.
/// Retirement frees the work, decrements the effect count and tears down the
/// task and coordinate body; callers must not retain pointers after retirement.
void shelterB1PodServiceGantryWaterDriftTaskU16(Task* task);

void shelterB1PodServiceGantryEffectSpriteRiseTask(Task* task);

/// Advances the pod service gantry's animated drifting sprite and releases it at completion.
///
/// Requires a live coordinate-body task with initialized `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` packs size in bits 0..11, period in
/// 12..14 (1 only when bits 12..15 are all zero), speed in 16..23 (0 means 64),
/// movement kind in 24..27, palette bank in 28..30, and alternate drawer in 31.
/// A nonzero period nibble must encode a nonzero period. Spin uses 4096 units
/// per turn; movement uses coordinate units per running update.
/// Initializes without drawing; later updates draw before moving, accelerating
/// and advancing through 12 banked or 10 alternate cells. Nonzero effect control
/// freezes updates but retains the drawer and palette; control >= 4 frees work
/// and task after that redraw. Drawing needs composed coordinates and scratch
/// and primitive-packet capacity.
void shelterB1PodServiceGantrySpriteDriftTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H
