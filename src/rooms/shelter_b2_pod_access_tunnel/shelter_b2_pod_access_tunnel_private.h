#ifndef SHELTER_B2_POD_ACCESS_TUNNEL_PRIVATE_H
#define SHELTER_B2_POD_ACCESS_TUNNEL_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_shelter_b2_pod_access_tunnel_8017D62C(Task *);
s32 func_shelter_b2_pod_access_tunnel_8017D7C4(Task *, s32, RoomEventMsg *, RoomEventMsg *);
void func_shelter_b2_pod_access_tunnel_8017D9A8(Task *);
s32 func_shelter_b2_pod_access_tunnel_8017DB28(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_b2_pod_access_tunnel_8017DB30(Task *, s32, s32, s32);
s32 func_shelter_b2_pod_access_tunnel_8017DB70(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_b2_pod_access_tunnel_8017DB78(Task *, s32, s32, GpMessageArg);

#endif // SHELTER_B2_POD_ACCESS_TUNNEL_PRIVATE_H
