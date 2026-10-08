#ifndef SRC_ROOMS_SHELTER_B1_ELEVATOR_HALL_SHELTER_B1_ELEVATOR_HALL_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_ELEVATOR_HALL_SHELTER_B1_ELEVATOR_HALL_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

#include "rooms/room_common.h"

extern TaskDesc D_shelter_b1_elevator_hall_80182CAC;

extern TaskMessageEntry D_shelter_b1_elevator_hall_80182CB8[6];

extern TaskDesc D_shelter_b1_elevator_hall_80182CE8;

extern RoomFadeStorage D_shelter_b1_elevator_hall_801849F0;

// Callbacks referenced by the overlay's shared data tables.

/// Resolves a departure from the hall and handles locked doors, lifts and mine transit.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with complete borrowed request/reply
/// records, which may alias. Copies the request before resolving the reply's
/// room. Returns 1 for direct travel, 2 for the deferred mine passage, and 0
/// for a refused door or the separately controlled elevator ride. Queries
/// suppress CAP, flag and task changes. Requires the Shelter map overlay.
/// Mine execution snapshots the destination and holds player control; do not
/// start another passage while `shelterB1ElevatorHallMineTransitTask` is live.
s32 shelterB1ElevatorHallResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Confirms passage to the mine, fades out and reloads the deferred destination.
///
/// Starts at state 0 with player control already held by the resolver. CAP
/// variant 10 confirms travel; other choices kill the task and resume control.
/// Confirmation pauses actors, waits three task ticks, records battle escape,
/// starts a 30-frame fade and waits for the transit sound before reloading.
/// Borrows the hall's singleton destination and fade storage until teardown;
/// the room overlay and CAP/sound resources must remain loaded throughout.
void shelterB1ElevatorHallMineTransitTask(Task* task);

/// Refuses every key-item use in the hall with the item menu cannot-use reply.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`. `itemId` is the selected collected-item
/// ID. All arguments are ignored; no inventory state changes.
s32 shelterB1ElevatorHallRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg);

/// Ignores the hall's room commands and returns zero.
///
/// Handles `ROOM_MESSAGE_COMMAND`; all arguments are unused.
s32 shelterB1ElevatorHallIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandMode);

/// Ignores the hall's direction actions and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`; the borrowed request is neither
/// read nor retained, and the other arguments are unused.
s32 shelterB1ElevatorHallIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* actionRequest, s32 secondArg);

/// Queues the hall's confirmation or elevator-ride sound for a room cue.
///
/// Handles `ROOM_MESSAGE_SOUND`: cue 6 plays the system confirmation sound,
/// cue 8 plays the hall's elevator-ride script, and other cues do nothing.
/// Always returns zero; the other arguments are unused. Requires the current
/// stage's sound scripts and the sound event queue.
s32 shelterB1ElevatorHallPlaySoundCueMessage(Task* task, s32 messageId, s32 cueId, s32 secondArg);

#endif // SRC_ROOMS_SHELTER_B1_ELEVATOR_HALL_SHELTER_B1_ELEVATOR_HALL_PRIVATE_H
