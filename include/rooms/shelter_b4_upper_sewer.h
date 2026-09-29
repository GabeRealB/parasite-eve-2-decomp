#ifndef ROOMS_SHELTER_B4_UPPER_SEWER_H
#define ROOMS_SHELTER_B4_UPPER_SEWER_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "actors/waypoints.h"
#include <psyq/libgte.h>

#include "common.h"

/// One water surface: a rectangle at (`x`, `z`) spanning `width` along X and
/// `depth` along Z, cut into `count` flat quads. The quads are laid along X
/// when `alongZ` is zero and along Z otherwise. A list of them ends at an entry
/// whose `count` is -1.
typedef struct ShelterB4UpperSewerSurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s16 count;
    s16 alongZ;
} ShelterB4UpperSewerSurface;

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b4_upper_sewer_801866F8[4];
extern SVECTOR D_shelter_b4_upper_sewer_80186708[9];
extern s16 D_shelter_b4_upper_sewer_80186438;

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
extern GpAreaVariant D_shelter_b4_upper_sewer_80188B9C[12];

#endif // ROOMS_SHELTER_B4_UPPER_SEWER_H
