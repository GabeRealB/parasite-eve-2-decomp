#ifndef INCLUDE_ROOMS_SHELTER_B1_POD_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B1_POD_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b1_pod_access_tunnel_80184C00[23];

// shelter_b1_pod_access_tunnel
extern u8* D_shelter_b1_pod_access_tunnel_80183A14[];

extern ViewCount D_shelter_b1_pod_access_tunnel_80183A18[];

extern DirectionWarpEntry D_shelter_b1_pod_access_tunnel_80183A1C[];

extern WorldCollisionGrid D_shelter_b1_pod_access_tunnel_80183C24;

extern ViewCamera D_shelter_b1_pod_access_tunnel_80183C48[];

extern SpriteView D_shelter_b1_pod_access_tunnel_8018462C[];

extern WorldCoordRoomLights D_shelter_b1_pod_access_tunnel_80184734;

extern WorldCollisionTrigger D_shelter_b1_pod_access_tunnel_8018474C[];

extern WorldCollisionOccluder D_shelter_b1_pod_access_tunnel_8018487C[];

extern WorldCollisionTrigger D_shelter_b1_pod_access_tunnel_801848B8[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_pod_access_tunnel_80184CDC[];

void func_shelter_b1_pod_access_tunnel_8017DEE8(Task* task);

void func_shelter_b1_pod_access_tunnel_8017E7D4(Task* arg0);

void func_shelter_b1_pod_access_tunnel_8017F138(Task* task);

void func_shelter_b1_pod_access_tunnel_8017FB9C(Task* task);

void func_shelter_b1_pod_access_tunnel_80180484(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_POD_ACCESS_TUNNEL_H
