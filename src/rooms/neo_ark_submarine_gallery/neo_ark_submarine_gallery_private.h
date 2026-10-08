#ifndef SRC_ROOMS_NEO_ARK_SUBMARINE_GALLERY_NEO_ARK_SUBMARINE_GALLERY_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_SUBMARINE_GALLERY_NEO_ARK_SUBMARINE_GALLERY_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskMessageEntry D_neo_ark_submarine_gallery_80181884[5];

extern TaskDesc D_neo_ark_submarine_gallery_801818AC;

// Callbacks referenced by the overlay's shared data tables.

void func_neo_ark_submarine_gallery_8017E86C(Task*);

/// Refuses every key-item use with the item menu's cannot-use reply.
///
/// `itemId` is the selected inventory item ID; all arguments are ignored.
s32 neoArkSubmarineGalleryRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);

s32 func_neo_ark_submarine_gallery_8017EA0C(Task*, s32, RoomEventMsg*, RoomEventMsg*);

/// Selects a gallery CAP event for room commands 2 and 3.
///
/// Command 2 selects an event by encounter activity; command 3 selects an event
/// by full-disc variant, engaged battle or idle room, in that order.
/// Starts the selected event only while CAP is idle. Other commands do nothing;
/// returns zero in every case. The receiver, message ID and final word are unused.
/// Requires gallery CAP commands 2..6 and their resources to remain loaded
/// through the queued event.
s32 neoArkSubmarineGalleryHandleCapCommand(Task* task, s32 messageId, s32 commandIndex, s32 unused);

/// Ignores room actions and returns zero without changing room state.
///
/// The borrowed request and the other callback arguments are unused.
s32 neoArkSubmarineGalleryIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);

/// Runs the red disc's radius and drawing lifecycle.
///
/// Requires a live task with state 0 (initialize radius), 1 (grow and draw), or
/// 2 (idle without release). The radius occupies `Task::killCountdown` in room
/// units, growing by 16 per active frame to 1920. Variant 4 starts at full size;
/// otherwise growth begins at zero. State 1 requires the room's live render
/// resources and does no work while the player task is absent. Keep this overlay
/// loaded throughout the task's lifetime.
void neoArkSubmarineGalleryRedDiscTask(Task* task);

#endif // SRC_ROOMS_NEO_ARK_SUBMARINE_GALLERY_NEO_ARK_SUBMARINE_GALLERY_PRIVATE_H
