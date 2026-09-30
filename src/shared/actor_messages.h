/* Standard handlers actors install in their message tables. Message 2004
 * places the model from a transform, in one variant also recording the
 * resulting yaw. Message 2005 shows, hides or buffer-flags the model. Each
 * package includes the handlers its table names.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_ACTOR_MESSAGES_H
#define SRC_SHARED_ACTOR_MESSAGES_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

/// The head of every work block actorMsgSetVisibility is used with: the
/// actor's state index, which the handler resets to 0 (or 0x18 when showing
/// the model). The rest of the block is the package's own.
typedef struct ActorStateWork {
    /* 0x0 */ s16 state;
} ActorStateWork;

/// The prefix of every work block actorMsgPlaceRecordYaw and
/// actorMsgPlaceYawFirst are used with: `yaw`
/// is the heading taken from the root coordinate's Z axis after the placement
/// rotations are applied.
typedef struct ActorYawWork {
    /* 0x00 */ byte pad_0[0x16];
    /* 0x16 */ s16  yaw;
} ActorYawWork;

s32 actorMsgPlace(Task* task, s32 arg1, ActorTransform* placement);
s32 actorMsgPlaceRecordYaw(Task* task, s32 arg1, ActorTransform* placement);
s32 actorMsgPlaceYawFirst(Task* task, s32 arg1, ActorTransform* placement);
s32 actorMsgSetVisibility(Task* task, s32 arg1, s32 arg2);

#endif /* SRC_SHARED_ACTOR_MESSAGES_H */
