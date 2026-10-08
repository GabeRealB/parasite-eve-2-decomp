#ifndef GAMEPLAY_PRIVATE_OBJECT_TASK_H
#define GAMEPLAY_PRIVATE_OBJECT_TASK_H

#include "gameplay/message.h"

#include "main/task_types.h"

void func_800E31E8(Task* arg0);

/// Plays a CAP command with optional actor pause, player hiding and action capture.
///
/// spawnArg1 contains CAP_EVENT_* bits; spawnArg2 is the loaded CAP command index.
/// Requires the command/resources to satisfy capRunCommand and remain loaded
/// through completion. This task starts immediately without reserving playback;
/// callers must serialize events. Waits for capIsBusy to clear, then restores
/// the requested actor/draw states and optionally sends commandIndex + 100 as
/// the room's sound cue. No work allocation or borrowed request is retained.
void capEventTask(Task* task);

/// Permits a room transition unchanged when no room-specific task is available.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` by copying all eight bytes from the
/// borrowed request to the writable reply and returning 1 (transition allowed).
/// Both pointers must address complete, two-byte-aligned `RoomEventMsg` storage;
/// they may be identical. Performs the same copy for queries and execution,
/// with no departure effects or flag updates, and retains neither pointer.
/// The receiver and message ID are unused callback-ABI arguments.
s32 objectTaskResolveDefaultRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Refuses every key-item use when no room-specific task is available.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; `itemId` is the selected collected-item id
/// and the sender supplies zero as the second payload. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED` without changing inventory or starting an event.
/// All callback arguments are unused.
s32 objectTaskRefuseDefaultRoomKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);

#endif // GAMEPLAY_PRIVATE_OBJECT_TASK_H
