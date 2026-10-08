/* The Dryfield cellar's message handlers, the same in the day and night builds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_CELLAR_H
#define SRC_SHARED_CELLAR_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

static s32 _cellarHandleCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg);
static s32 _roomVariantResolveCellar(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

#endif /* SRC_SHARED_CELLAR_H */
