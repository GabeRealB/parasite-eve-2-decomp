#ifndef SRC_ROOMS_MINE_SECRET_PASSAGE_MINE_SECRET_PASSAGE_PRIVATE_H
#define SRC_ROOMS_MINE_SECRET_PASSAGE_MINE_SECRET_PASSAGE_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

#include "rooms/room_common.h"

extern TaskMessageEntry D_mine_secret_passage_80180E8C[6];

extern TaskDesc D_mine_secret_passage_80180EBC;

extern RoomFadeStorage D_mine_secret_passage_80183440;

/// Confirms a passage departure and reloads after the exit-transit sound.
///
/// Requires a bodyless task in state 0..6, held player control, loaded CAP
/// command 2 and the destination already staged by the transition handler.
/// CAP key 10 confirms; any other key releases the task and resumes control.
/// Confirmation pauses actors, waits three callback ticks before battle-escape
/// processing, starts a 30-frame fade and waits for the transit sound to end.
/// Only then does it commit area/room/warp to the live save and queue reload
/// without repeating battle-escape processing. Fade/destination storage and the
/// room overlay must stay live through their use; only one departure may run.
void mineSecretPassageDepartureTask(Task* task);

/// Refuses every key-item use in the passage.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED` so the item menu displays its unavailable notice.
s32 mineSecretPassageRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);

/// Resolves passage departures and defers the elevator-hall route to a CAP prompt.
///
/// Borrows a complete eight-byte request and writable reply, which may alias.
/// Copies the request, then applies the loaded Mine/Shelter room-variant rules.
/// Elevator-hall requests return HANDLED (2) even for queries. Execution also stages
/// the resolved area's low byte, room and warp, holds player control and queues
/// the departure task; allocation failure still leaves control held. Other
/// routes return DIRECT (1). Task/message ID are ignored; no pointer is retained.
s32 mineSecretPassageResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Ignores room commands and returns zero without changing room state.
///
/// Handles `ROOM_MESSAGE_COMMAND`; the integer command and its argument,
/// task and message ID are all unused.
s32 mineSecretPassageIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);

/// Ignores direction-triggered room actions and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`; the borrowed request is neither
/// read nor retained. The task, message ID and second payload are unused.
s32 mineSecretPassageIgnoreAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

/// Requests the system confirmation sound for passage sound cue 3.
///
/// Handles `ROOM_MESSAGE_SOUND` synchronously and always returns zero. Other
/// integer cue IDs do nothing. The task, message ID and second payload are
/// unused; the queued sound is neither awaited nor owned by this callback.
s32 mineSecretPassageHandleSoundMessage(Task* task, s32 messageId, s32 cueId, s32 unusedArg);

#endif // SRC_ROOMS_MINE_SECRET_PASSAGE_MINE_SECRET_PASSAGE_PRIVATE_H
