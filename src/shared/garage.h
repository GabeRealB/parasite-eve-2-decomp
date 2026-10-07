/* The Dryfield garage's message handlers, the same in the day and night builds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GARAGE_H
#define SRC_SHARED_GARAGE_H

#include "types.h"

#include "main/task_types.h"

static s32 _garageSoundMsg(Task* task, s32 messageId, s32 cueKey, s32 unusedArg);

#endif /* SRC_SHARED_GARAGE_H */
