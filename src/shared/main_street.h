/* The Dryfield main street room in its day and night versions. It covers the
 * room's 0x13EE handler: it answers the neighbours' variants from story
 * nibbles, runs two staged events from its own gate and two item events
 * through roomEventGate. It also has the CAP cue sound handler, the 'talk'
 * handler that starts the play-time task, and the drifting puffs the room
 * task spawns as effect 0x601B1 by day and 0x601B2 by night. A puff is one
 * frame of a 10-cell 48x48 sheet on tpage 0x2B that drifts on a random bearing.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. A carrier of the puff task defines MAIN_STREET_PUFF_TASK as its
 * public void (Task*) callback name before including this header; its public
 * room header supplies the declaration.
 */

#ifndef SRC_SHARED_MAIN_STREET_H
#define SRC_SHARED_MAIN_STREET_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

/// Puff texture dimensions are in texels; rotation uses 4096 units per turn and Q12 trigonometry.
enum {
    MAIN_STREET_PUFF_CELLS_PER_ROW      = 5,
    MAIN_STREET_PUFF_CELL_WIDTH         = 48,
    MAIN_STREET_PUFF_UV_SPAN            = MAIN_STREET_PUFF_CELL_WIDTH - 1,
    MAIN_STREET_PUFF_TOP_V              = -128,
    MAIN_STREET_PUFF_MIN_DEPTH          = 65,
    MAIN_STREET_PUFF_QUARTER_TURN       = ONE / 4,
    MAIN_STREET_PUFF_TRIG_FRACTION_BITS = 12,
};

s32         mainStreetResolveMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out);
void        mainStreetPlayTimeTask(Task* task);
s32         mainStreetCapSoundCue(Task* task, s32 msgId, s32 arg2, s32 arg3);
s32         mainStreetTalkMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
static void _mainStreetDrawPuff(const GfxCoord* coord, u16 frame, s16 sizeFactor, s16 angle);

#endif /* SRC_SHARED_MAIN_STREET_H */
