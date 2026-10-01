/* The Dryfield garage's message handlers, the same in the day and night builds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GARAGE_H
#define SRC_SHARED_GARAGE_H

#include "types.h"

#include "main/task_types.h"

s32 garageSoundMsg(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3);

#endif /* SRC_SHARED_GARAGE_H */
