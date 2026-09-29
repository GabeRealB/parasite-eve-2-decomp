#ifndef ROOMS_NEO_ARK_PAVILION_H
#define ROOMS_NEO_ARK_PAVILION_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "actors/waypoints.h"
#include <psyq/libgte.h>

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_neo_ark_pavilion_80183A64[4];
extern SVECTOR D_neo_ark_pavilion_80183A74[8];
extern ActorWaypointHeight D_neo_ark_pavilion_801839A0;

extern TaskDesc D_neo_ark_pavilion_8018384C;
void func_neo_ark_pavilion_8017FC10(Task* arg0);
void func_neo_ark_pavilion_8017EC4C(Task* task);
void func_neo_ark_pavilion_8017F0CC(Task* task);
void func_neo_ark_pavilion_8017FCB0(Task* task);
void func_neo_ark_pavilion_80180714(Task* task);
void func_neo_ark_pavilion_80180FFC(Task* task);
void func_neo_ark_pavilion_80181C44(Task* arg0);
void func_neo_ark_pavilion_8018219C(Task* task);
void func_neo_ark_pavilion_80182DFC(Task* arg0);
extern GpAreaVariant D_neo_ark_pavilion_801876C4[13];

#endif
