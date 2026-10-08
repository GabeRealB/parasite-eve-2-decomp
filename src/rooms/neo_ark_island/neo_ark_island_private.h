#ifndef SRC_ROOMS_NEO_ARK_ISLAND_NEO_ARK_ISLAND_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_ISLAND_NEO_ARK_ISLAND_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

extern WorldCollisionGrid D_neo_ark_island_801826C8[1];

extern WorldCoordRoomLights D_neo_ark_island_80183CB0[1];

extern WorldCollisionTrigger D_neo_ark_island_80183CC8[4];

extern WorldCollisionTrigger D_neo_ark_island_80183DF8[3];

extern TaskMessageEntry D_neo_ark_island_80181B48[6];

extern TaskDesc D_neo_ark_island_80181B78;

// Callbacks referenced by the overlay's shared data tables.

void func_neo_ark_island_8017E844(Task*);

/// Refuses key-item use on the island without changing room state.
///
/// Installed for `ROOM_MESSAGE_USE_KEY_ITEM`. All arguments are ignored;
/// returns `ROOM_KEY_ITEM_USE_REFUSED` so the item menu shows its refusal.
s32 neoArkIslandRejectKeyItemMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);

s32 func_neo_ark_island_8017E968(Task*, s32, RoomEventMsg*, RoomEventMsg*);

/// Ignores island CAP room commands and returns zero.
///
/// Installed for `ROOM_MESSAGE_COMMAND`; `unusedCommandId` is the CAP command
/// selector. All arguments are ignored and no room state changes.
s32 neoArkIslandIgnoreCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedSecondArg);

/// Ignores the island's direction-trigger action requests and returns zero.
///
/// Installed for `DIRECTION_MESSAGE_ROOM_ACTION`. All arguments are ignored;
/// the borrowed request is neither accessed nor retained and no event starts.
s32 neoArkIslandIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

/// Maps island CAP sound cues to scripts in the island's room bank.
///
/// Installed for `ROOM_MESSAGE_SOUND`. Cue 3 plays bank entry 3; cue 101 plays
/// entry 4 only when the current CAP variant key is zero. Other cues do nothing.
/// Always returns zero; the remaining arguments are ignored. Queues sounds
/// with zero pan and depth while the room is loaded.
s32 neoArkIslandSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

#endif // SRC_ROOMS_NEO_ARK_ISLAND_NEO_ARK_ISLAND_PRIVATE_H
