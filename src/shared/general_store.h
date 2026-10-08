/* The Dryfield general store's events. The 0x13EE handler resolves two doors.
 * One (area 1) goes through the event gate with the store's sounds, its
 * destination variant taken from nibbles 0x63/0x7A/0x61. The other (area
 * 0x26), unless nibble 0x62 says to only show caption 0xE, latches the warp
 * and room and starts a cutscene task. That task takes the player's control
 * and weapon and forces view 0x10. It then plays caption 0xF with its sound.
 * If the caption's choice is key 0xB it fades out and moves the save to area
 * 0x26 at the latched warp and room; otherwise it restores everything. The
 * 0x13F0 handler shows a caption for action 0x18 and, for action 9, spawns a
 * small task that asks a caption question and toggles a story nibble on a yes.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GENERAL_STORE_H
#define SRC_SHARED_GENERAL_STORE_H

#include "types.h"

#include "gameplay/companion_load.h"

s32  storeDoorMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
void storeCutsceneTask(Task* arg0);
void storeToggleTask(Task* task);
s32  storeActionMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

#endif /* SRC_SHARED_GENERAL_STORE_H */
