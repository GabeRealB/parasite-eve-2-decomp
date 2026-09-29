#ifndef ROOMS_SHELTER_B2_MAIN_CORRIDOR_H
#define ROOMS_SHELTER_B2_MAIN_CORRIDOR_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "actors/waypoints.h"
#include <psyq/libgte.h>

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b2_main_corridor_80182EEC[4];
extern SVECTOR D_shelter_b2_main_corridor_80182EFC[12];
extern s16 D_shelter_b2_main_corridor_80182E28;

void func_shelter_b2_main_corridor_8017EF24(Task* task);
void func_shelter_b2_main_corridor_8017F3AC(Task* task);
void func_shelter_b2_main_corridor_8018094C(Task* task);
void func_shelter_b2_main_corridor_801813B0(Task* task);
void func_shelter_b2_main_corridor_80181C98(Task* task);
extern GpAreaVariant D_shelter_b2_main_corridor_8018933C[22];

void func_shelter_b2_main_corridor_8017EC34(Task* arg0);

#endif
