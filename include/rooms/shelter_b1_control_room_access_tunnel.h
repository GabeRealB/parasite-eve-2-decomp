#ifndef INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b1_control_room_access_tunnel_801825AC[22];

// shelter_b1_control_room_access_tunnel
extern u8* D_shelter_b1_control_room_access_tunnel_80181F00[];

extern ViewCount D_shelter_b1_control_room_access_tunnel_80181F04[];

extern GpWarpRec D_shelter_b1_control_room_access_tunnel_80181F08[];

extern WorldCollisionGrid D_shelter_b1_control_room_access_tunnel_80182070;

extern ViewCamera D_shelter_b1_control_room_access_tunnel_80182094[];

extern SpriteView D_shelter_b1_control_room_access_tunnel_80182130[];

extern WorldCoordRoomLights D_shelter_b1_control_room_access_tunnel_801822D4;

extern WorldCollisionTrigger D_shelter_b1_control_room_access_tunnel_801822EC[];

extern WorldCollisionTrigger D_shelter_b1_control_room_access_tunnel_80182384[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_control_room_access_tunnel_80182678[];

void func_shelter_b1_control_room_access_tunnel_8017D68C(Task* task);

void func_shelter_b1_control_room_access_tunnel_8017E1BC(Task* arg0);

void func_shelter_b1_control_room_access_tunnel_8017E2D8(Task* task);

void func_shelter_b1_control_room_access_tunnel_8017ED3C(Task* task);

void func_shelter_b1_control_room_access_tunnel_8017F624(Task* task);

void func_shelter_b1_control_room_access_tunnel_8018026C(Task* arg0);

void func_shelter_b1_control_room_access_tunnel_801807C4(Task* task);

void func_shelter_b1_control_room_access_tunnel_80181424(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_H
