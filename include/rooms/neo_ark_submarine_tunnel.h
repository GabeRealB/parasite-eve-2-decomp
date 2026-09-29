#ifndef ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H
#define ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "actors/waypoints.h"
#include <psyq/libgte.h>

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_neo_ark_submarine_tunnel_80181F94[4];
extern ActorWaypointHeight D_neo_ark_submarine_tunnel_80181E90;

extern TaskDesc D_neo_ark_submarine_tunnel_801810E4;
void func_neo_ark_submarine_tunnel_8017F4DC(Task* arg0);
void func_neo_ark_submarine_tunnel_8017FA34(Task* task);
void func_neo_ark_submarine_tunnel_80180694(Task* arg0);
extern GpAreaVariant D_neo_ark_submarine_tunnel_80187470[13];

void func_neo_ark_submarine_tunnel_8017F48C(Task* arg0);

#endif
