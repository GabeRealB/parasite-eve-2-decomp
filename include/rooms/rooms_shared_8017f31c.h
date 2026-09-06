#ifndef ROOMS_SHARED_8017F31C_H
#define ROOMS_SHARED_8017F31C_H

#include "common.h"

#include "main/task.h"

/// The room's "Notice" pop-up: `spawnArg1` picks one of three per-room message
/// blocks, the panel sizes itself to the text and closes on confirm, cancel or
/// after 0xBC frames; the menu button reports -1 upwards instead. Seven rooms
/// carry this body.
void RoomsShared8017f31c(Task* task);

/// Per-room text of the notice pop-up: the panel caption and the three
/// messages `spawnArg1` selects between.
extern u8 RoomsShared8017f31cNotice[];
extern u8 RoomsShared8017f31cMsg0[];
extern u8 RoomsShared8017f31cMsg1[];
extern u8 RoomsShared8017f31cMsg2[];

#endif // ROOMS_SHARED_8017F31C_H
