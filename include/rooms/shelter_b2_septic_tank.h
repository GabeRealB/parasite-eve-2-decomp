#ifndef ROOMS_SHELTER_B2_SEPTIC_TANK_H
#define ROOMS_SHELTER_B2_SEPTIC_TANK_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "actors/waypoints.h"
#include <psyq/libgte.h>

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b2_septic_tank_801836A4[4];
extern SVECTOR D_shelter_b2_septic_tank_801836B4[12];
extern s16 D_shelter_b2_septic_tank_801832BC;

void func_shelter_b2_septic_tank_8017EB7C(Task* arg0);
void func_shelter_b2_septic_tank_8017F040(Task* task);
void func_shelter_b2_septic_tank_8017F4C8(Task* task);
void func_shelter_b2_septic_tank_80180BE0(Task* task);
void func_shelter_b2_septic_tank_80181644(Task* task);
void func_shelter_b2_septic_tank_80181F2C(Task* task);
extern GpAreaVariant D_shelter_b2_septic_tank_80186F40[22];

#endif
