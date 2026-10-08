#ifndef SRC_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc gRoomEventTaskDesc;

extern TaskMessageEntry D_dryfield_night_water_tower_8017E6EC[6];

// Callbacks referenced by the overlay's shared data tables.

/// Refuses every key-item use without consuming the selected inventory item.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED` so the inventory menu displays its refusal notice.
s32 dryfieldNightWaterTowerRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);

/// Plays CAP slot 7's variant 3 for night-tower room command 7.
///
/// Starts slot 7 with variant key 3 and display-transition playback. Other
/// commands do nothing. Requires the room's relocated CAP resources; retains
/// no payload and always returns zero, including when playback cannot start.
s32 dryfieldNightWaterTowerCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);

/// Ignores trigger requests for room-specific actions and returns zero.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows `request` through synchronous
/// dispatch. No argument is read or retained and no room action is started.
s32 dryfieldNightWaterTowerIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H
