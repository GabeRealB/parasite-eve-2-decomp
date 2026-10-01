#ifndef INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H
#define INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H

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
extern SVECTOR* D_shelter_b4_upper_sewer_801866F8[4];

extern SVECTOR D_shelter_b4_upper_sewer_80186708[9];

extern s16 D_shelter_b4_upper_sewer_80186438;

extern GpAreaVariant D_shelter_b4_upper_sewer_80188B9C[12];

// shelter_b4_upper_sewer
extern u8* D_shelter_b4_upper_sewer_80186590[];

extern GpViewCountRec D_shelter_b4_upper_sewer_80186594[];

extern GpWarpRec D_shelter_b4_upper_sewer_80186598[];

extern WorldCollisionGrid D_shelter_b4_upper_sewer_80186EF8;

extern GpViewRec D_shelter_b4_upper_sewer_80186F1C[];

extern GpSprtRec D_shelter_b4_upper_sewer_801879BC[];

extern WorldCoordRoomLights D_shelter_b4_upper_sewer_80188184;

extern WorldCollisionTrigger D_shelter_b4_upper_sewer_8018819C[];

extern WorldCollisionTrigger D_shelter_b4_upper_sewer_801886F4[];

extern WorldCollisionOccluder D_shelter_b4_upper_sewer_80188BFC[];

extern WorldCollisionSurfaceProperties* D_shelter_b4_upper_sewer_80188CFC[];

void func_shelter_b4_upper_sewer_8017DC30(Task* task);

void func_shelter_b4_upper_sewer_80182734(Task* arg0);

void func_shelter_b4_upper_sewer_80183198(Task* task);

void func_shelter_b4_upper_sewer_80183A80(Task* task);

void func_shelter_b4_upper_sewer_8017E5F8(Task* arg0);

void func_shelter_b4_upper_sewer_8017E8B8(Task* task);

void func_shelter_b4_upper_sewer_8017ED40(Task* task);

void func_shelter_b4_upper_sewer_801846C8(Task* arg0);

void func_shelter_b4_upper_sewer_80184C20(Task* task);

void func_shelter_b4_upper_sewer_80185880(Task* task);

void func_shelter_b4_upper_sewer_80180110(Task* task);

void func_shelter_b4_upper_sewer_80180E58(Task* arg0);

void func_shelter_b4_upper_sewer_801811F0(Task* arg0);

void func_shelter_b4_upper_sewer_80182600(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H
