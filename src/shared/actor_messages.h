/* Standard handlers actors install in their message tables. Message 2004
 * places the model from a transform, in several variants that differ in how
 * the rotation is built, whether the yaw is recorded and what they return.
 * Message 2005 shows, hides or buffer-flags the model. Each package includes
 * the handlers its table names; a file with a second copy of one includes the
 * fragment again under that copy's name. actorMsgSetPairVisibility reads the
 * package's published tasks, gActorSelfTask and gActorHelperTask.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_ACTOR_MESSAGES_H
#define SRC_SHARED_ACTOR_MESSAGES_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

/// State indices the handlers test and store in `ActorMsgStateWork::state`.
///
/// A state index selects an entry of the package's own state table. Every
/// package that includes `actorMsgSetVisibility` gives `HIDDEN` and `PATROL`
/// these numbers, and every one that includes `actorMsgReleaseHold` the other
/// three; the rest of each table is the package's.
enum {
    ACTOR_MESSAGE_STATE_HIDDEN    = 0x00, // model not drawn; the actor waits to be shown
    ACTOR_MESSAGE_STATE_GRAB_HOLD = 0x0D, // holds the grabbed player
    ACTOR_MESSAGE_STATE_GRAB_DONE = 0x0E, // follows the hold once the player is let go
    ACTOR_MESSAGE_STATE_DORMANT   = 0x16, // idles in place until the player comes near
    ACTOR_MESSAGE_STATE_PATROL    = 0x18  // walks between its two patrol points, watching for the player
};

/// The start of an actor's work block as `actorMsgSetVisibility` and
/// `actorMsgReleaseHold` see it.
///
/// Each package's work block is a type of its own; these handlers are shared
/// between packages and know only that it opens with the state index. The
/// rest of the block is the package's.
typedef struct {
    s16 state; // index of the state handler the actor's tick runs; the handlers store `ACTOR_MESSAGE_STATE_*` values
} ActorMsgStateWork;

/// The start of an actor's work block as `actorMsgPlaceRecordYaw` and
/// `actorMsgPlaceYawFirst` see it.
///
/// Each package's work block is a type of its own; these handlers are shared
/// between packages and know only where it keeps the heading of the last
/// placement. The rest of the block is the package's.
typedef struct {
    byte packageFields[0x16]; // the package's own members ahead of the heading; the handlers touch none of them
    s16  placedYaw;           // heading the model root was left facing by the last placement message; 4096 units per turn
} ActorMsgYawWork;
STATIC_ASSERT_SIZEOF(ActorMsgYawWork, 0x18);

s32  actorMsgPlace(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);
s32  actorMsgPlaceRecordYaw(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);
s32  actorMsgPlaceYawFirst(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);
s32  actorMsgSetVisibility(Task* task, s32 arg1, s32 arg2, s32 arg3);
s32  actorMsgPlaceEuler(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);
s32  actorMsgPlaceRotMatrix(Task* arg0, s32 arg1, ActorTransform* args, s32 arg3);
void actorMsgPlaceYawPitchRoll(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);
s32  actorMsgPlaceInView(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);
s32  actorMsgSetPairVisibility(Task* task, s32 arg1, s32 flags, s32 arg3);
void actorMsgSetDrawMode(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

s32 actorMsgIsPresent(Task* task, s32 msgId, s32 arg2, s32 arg3);
s32 actorMsgReleaseHold(Task* task, s32 msgId, s32 arg2, s32 arg3);

s32 actorMsgPlaceEulerZyx(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);

#endif /* SRC_SHARED_ACTOR_MESSAGES_H */
