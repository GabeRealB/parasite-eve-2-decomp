#ifndef SRC_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
#define SRC_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_b2_north_maintenance_walkway_80183B48;

extern TaskDesc gRoomEventTaskDesc;

extern TaskMessageEntry D_shelter_b2_north_maintenance_walkway_80183B60[6];

// Callbacks referenced by the overlay's shared data tables.

/// Resolves walkway exits and stages the operating-room or breeding-room scene.
///
/// Borrows complete eight-byte request/reply records, which may alias, and
/// copies the request before resolving its room. Returns 1 for direct exits
/// and 2 for managed exits, including a missing operating-door collection bit.
/// Queries suppress scene starts. Executing that door scene identifies item
/// 0x122 after the gate records its start; deferred tasks own copied inputs.
s32 shelterB2NorthMaintenanceWalkwayResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Refuses every `ROOM_MESSAGE_USE_KEY_ITEM` request without consuming the item.
///
/// The menu supplies a collected-item ID and zero second payload; both are
/// ignored. Returns `ROOM_KEY_ITEM_USE_REFUSED` and changes no room state.
s32 shelterB2NorthMaintenanceWalkwayRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg);

/// Ignores `ROOM_MESSAGE_COMMAND` and both integer payloads, returning zero.
s32 shelterB2NorthMaintenanceWalkwayIgnoreCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 commandArg);

/// Starts the walkway's one-time scene for direction action 1 in room variant 1.
///
/// Borrows a four-byte `DirectionActionRequest` through dispatch. Starts normal
/// and skip scripts, records objective 0x20 and the seen flag, then reapplies
/// saved area updates. Other actions and visits are inert. Ignores the zero
/// second payload and returns zero; no request pointer is retained.
s32 shelterB2NorthMaintenanceWalkwayHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Queues this room's sound script 7 for sound cue 7; other cues are ignored.
///
/// `ROOM_MESSAGE_SOUND` supplies the integer cue key and an ignored second word.
/// Returns zero whether or not playback is queued; this does not wait for sound.
s32 shelterB2NorthMaintenanceWalkwayHandleSoundMessage(Task* unusedTask, s32 messageId, s32 cueKey, s32 unusedArg);

#endif // SRC_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
