#ifndef INCLUDE_ROOMS_ACROPOLIS_SQUARE_H
#define INCLUDE_ROOMS_ACROPOLIS_SQUARE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// The copied room-event handler uses this table only in stage 1, area 1.
extern GpAreaApplyRec D_acropolis_square_80188888[4];

extern GpAreaVariant D_acropolis_square_80185E50[3];

// acropolis_square
extern GpRoomObjRec D_acropolis_square_80183B9C[];

extern u8* D_acropolis_square_80183BAC[];

extern GpViewCountRec D_acropolis_square_80183BB0[];

extern GpRoomCoordRec D_acropolis_square_80183BB4[];

extern GpWarpRec D_acropolis_square_80183BBC[];

extern GpSprtRec D_acropolis_square_8018857C[];

extern GpViewRec D_acropolis_square_80188630[];

extern GpRoomParamRec* D_acropolis_square_80188868[];

s32 func_acropolis_square_80182360(s32 unused);

void func_acropolis_square_801823DC(Task* task);

void func_acropolis_square_801825DC(Task* task);

void func_acropolis_square_80180804(Task* task);

void func_acropolis_square_8017F41C(Task* task);

/// Task entries the Akropolis map UI overlay's stage tables name: each room's
/// entry task, started for its location, and the enemy descriptors' tasks.
void func_acropolis_square_80182308(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_SQUARE_H
