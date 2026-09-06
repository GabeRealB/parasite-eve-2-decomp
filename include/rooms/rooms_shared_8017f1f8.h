#ifndef ROOMS_SHARED_8017F1F8_H
#define ROOMS_SHARED_8017F1F8_H

#include "common.h"

#include "main/ui.h"

/// The first row of a save point's menu: draws the room's own label and, on
/// confirm, opens the room's panel, reports code 6 upwards and puts the owning
/// task into state 2. Nineteen rooms carry this body.
void RoomsShared8017f1f8(DialogPrompt* prompt, UiObject* obj);

/// Per-room data of that row: its label and the panel it opens.
extern u8           RoomsShared8017f1f8Msg[];
extern UiObjectDesc RoomsShared8017f1f8Desc;

#endif // ROOMS_SHARED_8017F1F8_H
