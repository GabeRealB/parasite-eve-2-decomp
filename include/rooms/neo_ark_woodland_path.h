#ifndef INCLUDE_ROOMS_NEO_ARK_WOODLAND_PATH_H
#define INCLUDE_ROOMS_NEO_ARK_WOODLAND_PATH_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_neo_ark_woodland_path_80181638;

extern GpAreaVariant D_neo_ark_woodland_path_8018471C[12];

// neo_ark_woodland_path
extern u8* D_neo_ark_woodland_path_80181694[];

extern GpViewCountRec D_neo_ark_woodland_path_80181698[];

extern GpWarpRec D_neo_ark_woodland_path_8018169C[];

extern GpGridParams D_neo_ark_woodland_path_80181D5C;

extern GpViewRec D_neo_ark_woodland_path_80181D80[];

extern GpSprtRec D_neo_ark_woodland_path_80183C6C[];

extern GpRoomCoordSet D_neo_ark_woodland_path_80183F84;

extern GpObj4A D_neo_ark_woodland_path_80183F9C[];

extern GpObj4A D_neo_ark_woodland_path_8018445C[];

extern WorldCoordRoomAmbientEntry D_neo_ark_woodland_path_8018477C[];

extern GpObj3A D_neo_ark_woodland_path_801847D4[];

extern WorldCollisionSurfaceProperties* D_neo_ark_woodland_path_80184910[];

void func_neo_ark_woodland_path_8017F4A0(Task* task);

void func_neo_ark_woodland_path_8017EA08(Task* task);

void func_neo_ark_woodland_path_8017F928(Task* task);

void func_neo_ark_woodland_path_8017ED00(Task* task);

void func_neo_ark_woodland_path_8017E9B0(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_WOODLAND_PATH_H
