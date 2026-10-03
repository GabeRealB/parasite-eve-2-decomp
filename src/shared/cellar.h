/* The Dryfield cellar's message handlers, the same in the day and night builds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_CELLAR_H
#define SRC_SHARED_CELLAR_H

#include "types.h"

#include "main/task_types.h"

s32 cellarCapMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
s32 cellarDoorMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

#endif /* SRC_SHARED_CELLAR_H */
