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

/// Resolves water-tower departures and starts the kitchen-door event when eligible.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` in the day and night rooms. Borrows
/// complete eight-byte request and reply records, which may alias, and copies
/// the request before resolving it. Queries suppress departure writes and
/// event starts. Executing a kitchen-door event identifies its required item;
/// other departures restore an operated tower mechanism and departure toward
/// the tank can clear companion schedule 7. Returns 0 to refuse, 1 for a
/// direct departure, or 2 for room-managed handling. Task and ID are unused.
/// Deferred event storage belongs to the room and must outlive the event task.
s32 roomVariantWaterTowerMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Plays the water-tower bank's stage-relative scripts for CAP sound cues 8 and 13.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cue keys have no effect. Borrows no
/// task state, ignores `messageId` and the second argument word, and returns
/// zero for every cue. The live session's stage selects the day or night bank.
s32 waterTowerSoundMsg(Task* task, s32 messageId, s32 cueKey, s32 unusedArg);

#endif /* SRC_SHARED_WATER_TOWER_H */
