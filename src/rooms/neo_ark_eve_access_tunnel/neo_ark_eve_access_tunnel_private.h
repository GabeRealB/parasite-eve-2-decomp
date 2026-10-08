#ifndef SRC_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

extern WorldCoordRoomLights D_neo_ark_eve_access_tunnel_801802D4[1];

extern WorldCollisionTrigger D_neo_ark_eve_access_tunnel_801802EC[6];

extern WorldCollisionTrigger D_neo_ark_eve_access_tunnel_801804B4[6];

extern WorldCollisionOccluder D_neo_ark_eve_access_tunnel_80180720[1];

extern TaskDesc D_neo_ark_eve_access_tunnel_8017EA88;

extern TaskMessageEntry D_neo_ark_eve_access_tunnel_8017EA94[6];

extern TaskDesc D_neo_ark_eve_access_tunnel_8017EAC4[3];

extern SpriteBatch D_neo_ark_eve_access_tunnel_8017F17C[2];

// Callbacks referenced by the overlay's shared data tables.

void func_neo_ark_eve_access_tunnel_8017D980(Task*);

void func_neo_ark_eve_access_tunnel_8017DB18(Task*);

s32 func_neo_ark_eve_access_tunnel_8017DC64(Task*, s32, s32, s32);

s32 func_neo_ark_eve_access_tunnel_8017DC6C(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_neo_ark_eve_access_tunnel_8017DD70(Task*, s32, s32, s32);

s32 func_neo_ark_eve_access_tunnel_8017DE1C(Task* task, s32 msgId, const void* firstArg, s32 arg3);

s32 func_neo_ark_eve_access_tunnel_8017DE9C(Task*, s32, s32, s32);

/// Records the tunnel's CAP completion in the supplied game-flag nibble.
///
/// While CAP is busy the live task waits. Once idle, variant key 12 preserves
/// the flag; every other key writes 2 to the flag ID in `spawnArg1.value`.
/// The task then releases itself regardless of the key. `spawnArg2` and state
/// are unused. The flag ID must be valid and the room overlay remain loaded.
void neoArkEveAccessTunnelRecordCapCompletionTask(Task* task);

#endif // SRC_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H
