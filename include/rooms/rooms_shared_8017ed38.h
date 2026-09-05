#ifndef ROOMS_SHARED_8017ED38_H
#define ROOMS_SHARED_8017ED38_H

#include "common.h"

#include "main/task.h"

/// Per-frame cursor driver of a room's action prompt, run as state 1 of the
/// room's prompt task. Seven rooms carry this body.
///
/// `Task::spawnArg1` picks which pad ports take part: 1 drives port 0 only,
/// 2 port 1 only, anything else both. For each port it integrates the analog
/// stick (pad status 0x12 reads it linearly, 0x73 squares it for a dead-zone
/// curve) and then the d-pad -- whose four bits select one of eight
/// 1/16-of-a-turn headings fed to `rsin`/`rcos` -- into the prompt's
/// 1/512-pixel position, clamps that to the screen, classifies the confirm
/// (0x40) and cancel (0xA0) buttons into the prompt's two button slots, and
/// finally hands the rounded position to `Room_Draw36` to draw the cursor.
/// `RoomActionPrompt::targetId` doubles as the cursor speed here and `field_E`
/// as the double-press window: a second press inside that many frames without
/// the cursor having moved reports state 4 instead of 2.
///
/// `step` carries the analog delta first and the d-pad heading afterwards, and
/// `idx` indexes the button slots in `u16` units so that `i` survives as the
/// loop counter.
void RoomsShared8017ed38(Task* task);

#endif // ROOMS_SHARED_8017ED38_H
