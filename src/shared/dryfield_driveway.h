/* Events of the Dryfield driveway (day and night packages). The room's resolve
 * message answers area 0x17 and 0x20 with the variant from progress nibbles.
 * Area 2 runs caption 6 once a nibble is set. At area 0x20 it spawns a two-
 * task cutscene on stage 2 variant 1 while nibble 0x50 is clear, otherwise
 * runs caption 1. At area 0x17 it either runs caption 2 or latches a staged
 * event (caption 9, flag 0x11C) for roomEventStagedTask. The cutscene tasks
 * either black out the display and run a two-block script, or run a scripted
 * scene and clear the area flag when it ends. A script hook plays two stage
 * sounds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_DRYFIELD_DRIVEWAY_H
#define SRC_SHARED_DRYFIELD_DRIVEWAY_H

#include "types.h"

#include "main/task_types.h"

s32        drivewayResolveEvent(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
void       drivewayBlackoutTask(Task* arg0);
void       drivewayCutsceneTask(Task* arg0);
static s32 _drivewayScriptSound(Task* task, s32 messageId, s32 cueKey, s32 unusedArg);

#endif /* SRC_SHARED_DRYFIELD_DRIVEWAY_H */
