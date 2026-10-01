#ifndef INCLUDE_ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H
#define INCLUDE_ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Models the Neo Ark map UI overlay's enemy descriptors attach.
extern TmdSource D_shelter_1f_vehicular_airlock_80182004;

extern GpAreaVariant D_shelter_1f_vehicular_airlock_80182A04[12];

// shelter_1f_vehicular_airlock
extern GpRoomObjRec D_shelter_1f_vehicular_airlock_801820FC[];

extern WorldCoordRoomLighting D_shelter_1f_vehicular_airlock_8018210C[];

extern u8* D_shelter_1f_vehicular_airlock_80182114[];

extern GpViewCountRec D_shelter_1f_vehicular_airlock_80182118[];

extern GpWarpRec D_shelter_1f_vehicular_airlock_8018211C[];

extern GpViewRec D_shelter_1f_vehicular_airlock_8018245C[];

extern SpriteView D_shelter_1f_vehicular_airlock_801824F8[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_vehicular_airlock_80182A80[];

void func_shelter_1f_vehicular_airlock_8017D5E4(Task* task);

void func_shelter_1f_vehicular_airlock_8017DA48(Task* task);

void func_shelter_1f_vehicular_airlock_8017ECBC(Task* task);

void func_shelter_1f_vehicular_airlock_8017F720(Task* task);

void func_shelter_1f_vehicular_airlock_80180008(Task* task);

void func_shelter_1f_vehicular_airlock_8017DAA0(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H
