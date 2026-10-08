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

s32 func_neo_ark_garden_8017E848(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_neo_ark_garden_8017E8DC(Task*, s32, s32, s32);

/// Ignores the garden's direction-trigger action requests and returns zero.
///
/// Installed for `DIRECTION_MESSAGE_ROOM_ACTION`. All arguments are ignored;
/// the borrowed request is neither accessed nor retained and no event starts.
s32 neoArkGardenIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

#endif // SRC_ROOMS_NEO_ARK_GARDEN_NEO_ARK_GARDEN_PRIVATE_H
