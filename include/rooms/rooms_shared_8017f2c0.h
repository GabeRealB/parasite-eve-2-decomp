#ifndef ROOMS_SHARED_8017F2C0_H
#define ROOMS_SHARED_8017F2C0_H

#include "common.h"

#include "main/ui.h"

/// The second row of a save point's menu, the twin of `RoomsShared8017f1f8`:
/// it draws the room's own label and opens the shared `RoomsShared8017f388`
/// panel instead of a room-specific one. Nineteen rooms carry this body.
void RoomsShared8017f2c0(DialogPrompt* prompt, UiObject* obj);

extern u8 RoomsShared8017f2c0Msg[];

#endif // ROOMS_SHARED_8017F2C0_H
