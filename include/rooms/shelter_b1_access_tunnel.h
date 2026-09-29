#ifndef INCLUDE_ROOMS_SHELTER_B1_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B1_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b1_access_tunnel_8017FE50[22];

// shelter_b1_access_tunnel
extern u8* D_shelter_b1_access_tunnel_8017E7E4[];

extern GpViewCountRec D_shelter_b1_access_tunnel_8017E7E8[];

extern GpWarpRec D_shelter_b1_access_tunnel_8017E7EC[];

extern GpGridParams D_shelter_b1_access_tunnel_8017EB24;

extern GpViewRec D_shelter_b1_access_tunnel_8017EB48[];

extern GpSprtRec D_shelter_b1_access_tunnel_8017F6A0[];

extern GpRoomCoordSet D_shelter_b1_access_tunnel_8017FA1C;

extern GpObj4A D_shelter_b1_access_tunnel_8017FA34[];

extern GpObj4A D_shelter_b1_access_tunnel_8017FBFC[];

extern GpObj3A D_shelter_b1_access_tunnel_8017FD2C[];

extern GpRoomParamRec* D_shelter_b1_access_tunnel_8017FF24[];

void func_shelter_b1_access_tunnel_8017DD08(Task* task);

void func_shelter_b1_access_tunnel_8017DD60(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_B1_ACCESS_TUNNEL_H
