#ifndef SRC_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_SHELTER_B2_POD_ACCESS_TUNNEL_PRIVATE_H
#define SRC_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_SHELTER_B2_POD_ACCESS_TUNNEL_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_b2_pod_access_tunnel_80183BC0;

extern TaskMessageEntry D_shelter_b2_pod_access_tunnel_80183BCC[6];

extern TaskDesc D_shelter_b2_pod_access_tunnel_80183BFC;

// Callbacks referenced by the overlay's shared data tables.

s32 func_shelter_b2_pod_access_tunnel_8017D7C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);

void func_shelter_b2_pod_access_tunnel_8017D9A8(Task*);

/// Refuses every `ROOM_MESSAGE_USE_KEY_ITEM` request without consuming the item.
///
/// The menu supplies a collected-item ID and zero second payload; both are
/// ignored. Returns `ROOM_KEY_ITEM_USE_REFUSED` and changes no room state.
s32 shelterB2PodAccessTunnelRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg);

s32 func_shelter_b2_pod_access_tunnel_8017DB30(Task*, s32, s32, s32);

/// Ignores `DIRECTION_MESSAGE_ROOM_ACTION` and returns zero without room changes.
///
/// The direction system supplies a borrowed four-byte request and zero second
/// payload. Neither is read or retained.
s32 shelterB2PodAccessTunnelIgnoreActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* unusedRequest, s32 unusedArg);

/// Queues the system confirmation sound for room sound cue 4; other cues are ignored.
///
/// `ROOM_MESSAGE_SOUND` supplies the integer cue key and an ignored second word.
/// Returns zero whether or not playback is queued; this does not wait for sound.
s32 shelterB2PodAccessTunnelHandleSoundMessage(Task* unusedTask, s32 messageId, s32 cueKey, s32 unusedArg);

#endif // SRC_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_SHELTER_B2_POD_ACCESS_TUNNEL_PRIVATE_H
