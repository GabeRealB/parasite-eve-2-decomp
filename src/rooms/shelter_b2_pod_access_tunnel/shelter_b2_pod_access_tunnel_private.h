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

/// Resolves R48's locked door and the septic-tank departure event.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with complete eight-byte request/reply
/// records, which may alias. Copies the request before resolving the reply's
/// room. Returns 0 for locked R48, 1 for direct departure, or 2 when the septic
/// prompt/event handles departure. Queries suppress CAP and event latch effects;
/// the staged-event gate still clears its latest-start indication. Execution
/// at locked R48 writes 2 to the optional request flag. Deferred execution
/// copies the reply and event into room-owned state; no caller pointer is kept.
/// Keep the room, map resolver and CAP/sound resources loaded until the event
/// ends. Receiver and message ID are unused.
s32 shelterB2PodAccessTunnelResolveRoomEventMessage(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Prompts for a pod ride and reloads into B1 pod access tunnel, room 1, arrival 3.
///
/// Start as a bodyless task in state 0 with player scripted control held.
/// States 0..4 prompt, wait CAP, interpret the choice, wait the ride sound and
/// depart. CAP key 10 accepts; key 1 declines and marks the pod, other keys
/// only decline. Declining resumes actors, releases this task, then resumes the
/// player. Acceptance holds control through reload, clearing the pod map mark.
/// The room and CAP/sound resources must remain loaded until this task ends;
/// spawn arguments are unused. A failed reload spawn still commits arrival.
void shelterB2PodAccessTunnelRideToB1Task(Task* task);

/// Refuses every `ROOM_MESSAGE_USE_KEY_ITEM` request without consuming the item.
///
/// The menu supplies a collected-item ID and zero second payload; both are
/// ignored. Returns `ROOM_KEY_ITEM_USE_REFUSED` and changes no room state.
s32 shelterB2PodAccessTunnelRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg);

/// Holds player control and starts the B1 pod-ride task for room command 1.
///
/// `ROOM_MESSAGE_COMMAND` supplies an integer command ID; other commands are
/// ignored and all return zero. The receiver, message ID and second word are
/// unused. No payload is retained. Requires this room's task/CAP/sound resources
/// loaded through the ride. Player control stays held if task allocation fails.
s32 shelterB2PodAccessTunnelHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg);

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
