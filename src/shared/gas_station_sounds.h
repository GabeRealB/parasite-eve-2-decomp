/* The gas station's caption sound cues. The room's 0x13F2 handler maps a
 * caption script's cue key to a stage sound in bank 0x5201 and queues it. The
 * pair's other function, the 0x13EE resolver, goes to room_variants (above).
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GAS_STATION_SOUNDS_H
#define SRC_SHARED_GAS_STATION_SOUNDS_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

static s32 _gasStationCueSoundMsg(Task* task, s32 messageId, s32 cueKey, s32 unusedArg);

#endif /* SRC_SHARED_GAS_STATION_SOUNDS_H */
