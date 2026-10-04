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

void func_shelter_b1_pod_service_gantry_8017FA7C(Task* arg0);

void func_shelter_b1_pod_service_gantry_8017E880(Task* task);

void effectSpriteRiseTask(Task* task);

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
