#ifndef SRC_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_SHELTER_B2_POD_ACCESS_TUNNEL_PRIVATE_H
#define SRC_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_SHELTER_B2_POD_ACCESS_TUNNEL_PRIVATE_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_b2_pod_access_tunnel_80183BC0;

extern TaskMessageEntry D_shelter_b2_pod_access_tunnel_80183BCC[6];

extern TaskDesc D_shelter_b2_pod_access_tunnel_80183BFC;

// Callbacks referenced by the overlay's shared data tables.

s32 func_shelter_b2_pod_access_tunnel_8017D7C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);

void func_shelter_b2_pod_access_tunnel_8017D9A8(Task*);

s32 func_shelter_b2_pod_access_tunnel_8017DB28(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_shelter_b2_pod_access_tunnel_8017DB30(Task*, s32, s32, s32);

s32 func_shelter_b2_pod_access_tunnel_8017DB70(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_shelter_b2_pod_access_tunnel_8017DB78(Task*, s32, s32, TaskMessageArg);

#endif // SRC_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_SHELTER_B2_POD_ACCESS_TUNNEL_PRIVATE_H
