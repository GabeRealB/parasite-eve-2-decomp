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

/// Plays the water-tower bank's stage-relative scripts for CAP sound cues 8 and 13.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cue keys have no effect. Borrows no
/// task state, ignores `messageId` and the second argument word, and returns
/// zero for every cue. The live session's stage selects the day or night bank.
s32 waterTowerSoundMsg(Task* task, s32 messageId, s32 cueKey, s32 unusedArg);

#endif /* SRC_SHARED_WATER_TOWER_H */
