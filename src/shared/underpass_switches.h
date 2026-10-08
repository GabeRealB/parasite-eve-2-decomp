/* The Dryfield underpass's two switches. Message 0x13F0 action 1 or 2 spawns a
 * task that asks a caption question (1 or 2) and, on a yes (event key >= 0xA),
 * toggles story nibble 0x51 or 0x52. Toggling 0x51 also reworks which variant
 * of the general store (area 0x26) the session and save load, from nibbles
 * 0xC9/0x53/0x51. A variant of 5 or more marks the view dirty. The rooms'
 * 0x13EE resolver for areas 0x20/0x22, driven by the same nibbles, goes to
 * room_variants (above).
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_UNDERPASS_SWITCHES_H
#define SRC_SHARED_UNDERPASS_SWITCHES_H

#include "types.h"

#include "main/task_types.h"

void underpassSwitchTask(Task* task);
s32  underpassSwitchMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

#endif /* SRC_SHARED_UNDERPASS_SWITCHES_H */
