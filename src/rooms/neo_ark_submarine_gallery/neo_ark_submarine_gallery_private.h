#ifndef SRC_ROOMS_NEO_ARK_SUBMARINE_GALLERY_NEO_ARK_SUBMARINE_GALLERY_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_SUBMARINE_GALLERY_NEO_ARK_SUBMARINE_GALLERY_PRIVATE_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskMessageEntry D_neo_ark_submarine_gallery_80181884[5];

extern TaskDesc D_neo_ark_submarine_gallery_801818AC;

// Callbacks referenced by the overlay's shared data tables.

void func_neo_ark_submarine_gallery_8017E86C(Task*);

s32 func_neo_ark_submarine_gallery_8017EA04(Task*, s32, s32, s32);

s32 func_neo_ark_submarine_gallery_8017EA0C(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_neo_ark_submarine_gallery_8017EABC(Task*, s32, s32, s32);

s32 func_neo_ark_submarine_gallery_8017EB48(Task*, s32, s32, s32);

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
