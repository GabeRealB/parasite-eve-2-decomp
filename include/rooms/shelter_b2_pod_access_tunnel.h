#ifndef INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b2_pod_access_tunnel_801855AC[23];

// shelter_b2_pod_access_tunnel
extern WorldCollisionRoomResources D_shelter_b2_pod_access_tunnel_80183DEC[];

extern WorldCoordRoomLighting D_shelter_b2_pod_access_tunnel_80183E0C[];

extern u8* D_shelter_b2_pod_access_tunnel_80183E24[];

extern ViewCount D_shelter_b2_pod_access_tunnel_80183E2C[];

extern DirectionWarpEntry D_shelter_b2_pod_access_tunnel_80183E30[];

extern ViewCamera D_shelter_b2_pod_access_tunnel_801841D8[];

extern SpriteView D_shelter_b2_pod_access_tunnel_80184C6C[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_pod_access_tunnel_801856D8[];

void func_shelter_b2_pod_access_tunnel_8017DC14(Task* task);

void shelterB2PodAccessTunnelEffectSpriteDriftTask(Task* task);

void func_shelter_b2_pod_access_tunnel_80181C2C(Task* arg0);

void func_shelter_b2_pod_access_tunnel_80182690(Task* task);

void func_shelter_b2_pod_access_tunnel_80182F78(Task* task);

void func_shelter_b2_pod_access_tunnel_8017F608(Task* task);

void func_shelter_b2_pod_access_tunnel_80180350(Task* arg0);

void func_shelter_b2_pod_access_tunnel_801806E8(Task* arg0);

void func_shelter_b2_pod_access_tunnel_80181AF8(Task* arg0);

void func_shelter_b2_pod_access_tunnel_8017DC6C(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H
