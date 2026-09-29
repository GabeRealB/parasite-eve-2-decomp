#ifndef NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H
#define NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "gameplay/direction.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_neo_ark_eve_access_tunnel_8017D810(Task *);
void func_neo_ark_eve_access_tunnel_8017D980(Task *);
void func_neo_ark_eve_access_tunnel_8017DB18(Task *);
s32 func_neo_ark_eve_access_tunnel_8017DC64(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_eve_access_tunnel_8017DC6C(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_neo_ark_eve_access_tunnel_8017DD70(Task *, s32, s32, GpMessageArg);
s32 func_neo_ark_eve_access_tunnel_8017DE1C(Task *, s32, GpMsg13EF *, GpMessageArg);
s32 func_neo_ark_eve_access_tunnel_8017DE9C(Task *, s32, s32, GpMessageArg);
void func_neo_ark_eve_access_tunnel_8017DED0(Task *);

#endif // NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H
