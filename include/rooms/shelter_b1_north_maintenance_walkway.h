#ifndef ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_H
#define ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "common.h"

#include <psyq/libgte.h>

/// Queues a grey gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`, of radius `arg1` turned by the angle `arg2`.
void func_shelter_b1_north_maintenance_walkway_8017DDE0(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Queues a red gouraud disc at the projected point `arg0`, of radius `arg1`.
void func_shelter_b1_north_maintenance_walkway_8017E55C(SVECTOR* arg0, s16 arg1);

void func_shelter_b1_north_maintenance_walkway_8017E8B8(Task* task);
void func_shelter_b1_north_maintenance_walkway_80180EDC(Task* arg0);
void func_shelter_b1_north_maintenance_walkway_80181940(Task* task);
void func_shelter_b1_north_maintenance_walkway_80182228(Task* task);
void func_shelter_b1_north_maintenance_walkway_80182E70(Task* arg0);
void func_shelter_b1_north_maintenance_walkway_801833C8(Task* task);
void func_shelter_b1_north_maintenance_walkway_80184028(Task* arg0);
void func_shelter_b1_north_maintenance_walkway_8017F600(Task* arg0);
void func_shelter_b1_north_maintenance_walkway_8017F998(Task* arg0);
void func_shelter_b1_north_maintenance_walkway_80180DA8(Task* arg0);
extern GpAreaVariant D_shelter_b1_north_maintenance_walkway_80185A38[12];

void func_shelter_b1_north_maintenance_walkway_8017DBC8(Task* arg0);

#endif // ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_H
