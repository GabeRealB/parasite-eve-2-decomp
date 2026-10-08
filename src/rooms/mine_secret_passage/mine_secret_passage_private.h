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

// Callbacks referenced by the overlay's shared data tables.
void func_mine_secret_passage_8017D60C(Task*);

/// Refuses every key-item use in the passage.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED` so the item menu displays its unavailable notice.
s32 mineSecretPassageRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);

s32 func_mine_secret_passage_8017D7CC(Task*, s32, RoomEventMsg*, RoomEventMsg*);

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
