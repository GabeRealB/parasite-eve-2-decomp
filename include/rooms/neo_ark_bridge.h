#ifndef ROOMS_NEO_ARK_BRIDGE_H
#define ROOMS_NEO_ARK_BRIDGE_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "actors/waypoints.h"
#include <psyq/libgte.h>

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_neo_ark_bridge_801820BC[4];
extern SVECTOR D_neo_ark_bridge_801820CC[9];
extern ActorWaypointHeight D_neo_ark_bridge_80181FF8;

extern TaskDesc D_neo_ark_bridge_80181F18;
void func_neo_ark_bridge_8017E954(Task* arg0);
void func_neo_ark_bridge_8017EF70(Task* task);
void func_neo_ark_bridge_8017F3F8(Task* task);
void func_neo_ark_bridge_8017FF84(Task* task);
void func_neo_ark_bridge_801809E8(Task* task);
void func_neo_ark_bridge_801812D0(Task* task);
extern GpAreaVariant D_neo_ark_bridge_80184A50[13];

#endif
