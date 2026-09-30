/* The Dryfield main street room in its day and night versions. It covers the
 * room's 0x13EE handler: it answers the neighbours' variants from story
 * nibbles, runs two staged events from its own gate and two item events
 * through roomEventGate. It also has the CAP cue sound handler, the 'talk'
 * handler that starts the play-time task, and the drifting smoke puffs the
 * room task spawns as effect 0x601B2. A puff is one frame of a 10-cell 48x48
 * sheet on tpage 0x2B that drifts on a random bearing.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_MAIN_STREET_H
#define SRC_SHARED_MAIN_STREET_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

s32  mainStreetResolveMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out);
void mainStreetPlayTimeTask(Task* task);
s32  mainStreetCapSoundCue(Task* task, s32 msgId, s32 arg2, s32 arg3);
s32  mainStreetTalkMsg(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3);
void mainStreetPuffTask(Task* task);
void mainStreetDrawPuff(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);

#endif /* SRC_SHARED_MAIN_STREET_H */
