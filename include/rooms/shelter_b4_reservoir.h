#ifndef ROOMS_SHELTER_B4_RESERVOIR_H
#define ROOMS_SHELTER_B4_RESERVOIR_H

#include "types.h"

#include "gameplay/area.h"

#include "main/task_types.h"

#include "actors/waypoints.h"
#include <psyq/libgte.h>

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b4_reservoir_801851D4[4];
extern SVECTOR D_shelter_b4_reservoir_801851E4[5];
extern s16 D_shelter_b4_reservoir_80184F80;

void func_shelter_b4_reservoir_801813F0(Task* task);
void func_shelter_b4_reservoir_8017FB84(Task* task);
void func_shelter_b4_reservoir_801803DC(Task* task);
void func_shelter_b4_reservoir_80180864(Task* task);
void func_shelter_b4_reservoir_80182B1C(Task* arg0);
void func_shelter_b4_reservoir_80183074(Task* task);
void func_shelter_b4_reservoir_80183CD4(Task* arg0);
extern GpAreaVariant D_shelter_b4_reservoir_80187350[12];

extern u8 D_shelter_b4_reservoir_801850C8[16];
extern u8 D_shelter_b4_reservoir_801850D8[16];

#endif
