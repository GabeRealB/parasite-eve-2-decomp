#ifndef ROOMS_SHELTER_B4_LOWER_SEWER_H
#define ROOMS_SHELTER_B4_LOWER_SEWER_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "actors/waypoints.h"
#include <psyq/libgte.h>

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b4_lower_sewer_8018210C[4];
extern SVECTOR D_shelter_b4_lower_sewer_8018211C[10];
extern s16 D_shelter_b4_lower_sewer_80181E6C;

void func_shelter_b4_lower_sewer_8017FEB0(Task* task);
void func_shelter_b4_lower_sewer_80180914(Task* task);
void func_shelter_b4_lower_sewer_801811FC(Task* task);
void func_shelter_b4_lower_sewer_8017EEE4(Task* task);
void func_shelter_b4_lower_sewer_8017F36C(Task* task);
extern GpAreaVariant D_shelter_b4_lower_sewer_80183D48[12];

void func_shelter_b4_lower_sewer_8017E400(Task* arg0);

#endif
