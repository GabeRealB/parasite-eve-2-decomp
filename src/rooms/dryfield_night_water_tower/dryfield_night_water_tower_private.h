#ifndef SRC_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Bodyless transition-task recipe shared by this overlay's gate and event task.
///
/// Runs at priority `TASK_PRIORITY_SCENE` with zero spawn payloads and unused metadata. The gate
/// reads the recipe synchronously after latching the request and destination;
/// keep those snapshots and the room callback code live through session reload.
extern TaskDesc gRoomEventTaskDesc;

/// Message handlers borrowed by the night Water Tower's registered room task.
///
/// Resolves departures, refuses key-item use, ignores room actions, plays
/// sound cues and handles room commands. The sixth entry ends the search, so
/// unsupported IDs return zero. Keep the table and callbacks loaded while
/// the room task can receive messages; never dispatch `TASK_MESSAGE_TABLE_END`.
extern TaskMessageEntry gDryfieldNightWaterTowerRoomMessages[6];

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
