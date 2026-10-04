#ifndef INCLUDE_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H
#define INCLUDE_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_neo_ark_submarine_tunnel_80181F94[4];

extern s16 D_neo_ark_submarine_tunnel_80181E90;

extern TaskDesc D_neo_ark_submarine_tunnel_801810E4;

extern AreaVariant D_neo_ark_submarine_tunnel_80187470[13];

// neo_ark_submarine_tunnel
extern WorldCoordRoomLighting D_neo_ark_submarine_tunnel_80181E00[];

extern WorldCollisionRoomResources D_neo_ark_submarine_tunnel_80181E08[];

extern u8* D_neo_ark_submarine_tunnel_80181E18[];

extern ViewCount D_neo_ark_submarine_tunnel_80181E1C[];

extern DirectionWarpEntry D_neo_ark_submarine_tunnel_80181E20[];

extern ViewCamera D_neo_ark_submarine_tunnel_80182500[];

extern SpriteView D_neo_ark_submarine_tunnel_80186B78[];

extern WorldCollisionSurfaceProperties* D_neo_ark_submarine_tunnel_801878EC[];

void func_neo_ark_submarine_tunnel_8017F4DC(Task* arg0);

void func_neo_ark_submarine_tunnel_8017FA34(Task* task);

void func_neo_ark_submarine_tunnel_80180694(Task* arg0);

void func_neo_ark_submarine_tunnel_8017F48C(Task* arg0);

void func_neo_ark_submarine_tunnel_8017F434(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H
