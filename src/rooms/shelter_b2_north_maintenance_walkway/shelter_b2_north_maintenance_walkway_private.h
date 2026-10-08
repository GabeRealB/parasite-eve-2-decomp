#ifndef SRC_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
#define SRC_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_b2_north_maintenance_walkway_80183B48;

extern TaskDesc gRoomEventTaskDesc;

extern TaskMessageEntry D_shelter_b2_north_maintenance_walkway_80183B60[6];

// Callbacks referenced by the overlay's shared data tables.

s32 func_shelter_b2_north_maintenance_walkway_8017DA88(Task*, s32, RoomEventMsg*, RoomEventMsg*);

/// Refuses every `ROOM_MESSAGE_USE_KEY_ITEM` request without consuming the item.
///
/// The menu supplies a collected-item ID and zero second payload; both are
/// ignored. Returns `ROOM_KEY_ITEM_USE_REFUSED` and changes no room state.
s32 shelterB2NorthMaintenanceWalkwayRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg);

/// Ignores `ROOM_MESSAGE_COMMAND` and both integer payloads, returning zero.
s32 shelterB2NorthMaintenanceWalkwayIgnoreCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 commandArg);

s32 func_shelter_b2_north_maintenance_walkway_8017DC54(Task*, s32, RoomEventMsg*, RoomEventMsg*);

/// Queues this room's sound script 7 for sound cue 7; other cues are ignored.
///
/// `ROOM_MESSAGE_SOUND` supplies the integer cue key and an ignored second word.
/// Returns zero whether or not playback is queued; this does not wait for sound.
s32 shelterB2NorthMaintenanceWalkwayHandleSoundMessage(Task* unusedTask, s32 messageId, s32 cueKey, s32 unusedArg);

#endif // SRC_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
