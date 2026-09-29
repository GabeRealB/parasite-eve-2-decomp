#ifndef ROOMS_NEO_ARK_ISLAND_H
#define ROOMS_NEO_ARK_ISLAND_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "actors/waypoints.h"
#include <psyq/libgte.h>

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_neo_ark_island_80181CE8[4];
extern SVECTOR D_neo_ark_island_80181CF8[7];
extern ActorWaypointHeight D_neo_ark_island_80181C24;

extern TaskDesc D_neo_ark_island_80181B30;
void func_neo_ark_island_8017FB2C(Task* arg0);
void func_neo_ark_island_8017EB68(Task* task);
void func_neo_ark_island_8017EFE8(Task* task);
void func_neo_ark_island_8017FB9C(Task* task);
void func_neo_ark_island_80180600(Task* task);
void func_neo_ark_island_80180EE8(Task* task);
extern GpAreaVariant D_neo_ark_island_80183F48[13];

#endif
