/* The Dryfield toilet's message handlers, the same in the day and night builds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_TOILET_H
#define SRC_SHARED_TOILET_H

#include "types.h"

#include "main/task_types.h"

s32 toiletSoundMsg(Task* task, s32 msgId, s32 arg2, s32 arg3);

#endif /* SRC_SHARED_TOILET_H */
