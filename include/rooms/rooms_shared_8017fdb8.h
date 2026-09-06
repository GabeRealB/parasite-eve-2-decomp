#ifndef ROOMS_SHARED_8017FDB8_H
#define ROOMS_SHARED_8017FDB8_H

#include "common.h"

#include "main/ui.h"

/// One row of a room's list panel: draws the room's own `RoomsShared8017fdb8Msg`
/// text at the row's offset, one OT slot in front of the panel, and reports
/// code 6 upwards when the highlighted row is confirmed. Seven rooms carry this
/// body, each with its own label bytes.
void RoomsShared8017fdb8(DialogPrompt* prompt, UiObject* obj);

extern u8 RoomsShared8017fdb8Msg[];

#endif // ROOMS_SHARED_8017FDB8_H
