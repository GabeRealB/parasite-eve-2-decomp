/* The Dryfield water tower's message handlers, the same in the day and night builds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_WATER_TOWER_H
#define SRC_SHARED_WATER_TOWER_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

s32 waterTowerEventMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out);
s32 waterTowerSoundMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

#endif /* SRC_SHARED_WATER_TOWER_H */
