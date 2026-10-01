/* The Dryfield back street's events, the same in the day and night builds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_BACK_STREET_H
#define SRC_SHARED_BACK_STREET_H

#include "types.h"

#include "main/task_types.h"

s32 backStreetEventMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

#endif /* SRC_SHARED_BACK_STREET_H */
