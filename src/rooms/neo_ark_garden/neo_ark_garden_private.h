#ifndef SRC_ROOMS_NEO_ARK_GARDEN_NEO_ARK_GARDEN_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_GARDEN_NEO_ARK_GARDEN_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskMessageEntry D_neo_ark_garden_801813B0[5];

// Callbacks referenced by the overlay's shared data tables.

/// Refuses key-item use in the garden without changing room state.
///
/// Installed for `ROOM_MESSAGE_USE_KEY_ITEM`. All arguments are ignored;
/// returns `ROOM_KEY_ITEM_USE_REFUSED` so the item menu shows its refusal.
s32 neoArkGardenRejectKeyItemMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);

/// Resolves room selection and blocks the substation departure until altar sequence 1 is solved.
///
/// Copies the complete borrowed request to the writable reply, which may alias it.
/// Returns 1 for other areas or a solved altar, otherwise 0, including queries.
/// Only an execute request on that blocked path marks the optional map flag with
/// 2 and runs CAP 1. Neither record is retained; the receiver and message ID are unused.
s32 neoArkGardenResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Handles the garden's room commands and progress-dependent CAP requests.
///
/// Command 4 selects CAP 4 or 6 and applies the saved-area updates once while
/// altar sequence 1 remains unsolved. Commands 7 and 5 attempt CAP 7/9 and 5/10
/// respectively, selecting the latter after nursery progress and spawning only
/// when CAP is idle. Other commands do nothing. Always returns zero; unused
/// arguments carry no payload ownership.
s32 neoArkGardenHandleCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);

/// Ignores the garden's direction-trigger action requests and returns zero.
///
/// Installed for `DIRECTION_MESSAGE_ROOM_ACTION`. All arguments are ignored;
/// the borrowed request is neither accessed nor retained and no event starts.
s32 neoArkGardenIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

#endif // SRC_ROOMS_NEO_ARK_GARDEN_NEO_ARK_GARDEN_PRIVATE_H
