#ifndef GAMEPLAY_PRIVATE_OBJECT_TASK_H
#define GAMEPLAY_PRIVATE_OBJECT_TASK_H

#include "gameplay/message.h"

#include "main/task_types.h"

/// Resets room presentation/music state and selects the loaded map's room task.
///
/// State 0 of `objectTaskRoomTask`; neither spawn argument is consumed.
/// Requires a live bodyless task and session with stage 1..5, area/room IDs
/// below 100 and a loaded, TASK_DESC_END-terminated stage descriptor table.
/// The decimal key is stage * 10000 + area * 100 + room; room zero denotes
/// the area default. Selects the first bodyless priority-32 descriptor matching
/// the room key or area key, in table order, and attempts to spawn it with zero
/// arguments. Allocation failure still advances this selector to state 1.
/// With no match, installs the default transition/key-item message receiver and
/// publishes this task in GAME_TASK_SLOT_ROOM. The slot borrows the task through
/// teardown; keep the selected map/room callbacks loaded while they can run.
void objectTaskInitializeRoomState(Task* task);

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
