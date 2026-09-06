#ifndef ROOMS_SHARED_8017F108_H
#define ROOMS_SHARED_8017F108_H

#include "common.h"

#include "main/ui.h"

/// The vending machine's confirm row: draws the room's own prompt text and, on
/// confirm, either buys the item outright, opens the quantity panel when the
/// row's mode differs from the machine's, or raises the notice pop-up when the
/// player cannot afford it or cannot carry it. Seven rooms carry this body.
void RoomsShared8017f108(DialogPrompt* prompt, UiObject* obj);

/// Per-room data of the confirm row: its prompt text and the notice and
/// quantity panels it spawns.
extern u8           RoomsShared8017f108Msg[];
extern UiObjectDesc RoomsShared8017f108NoticeDesc;
extern UiObjectDesc RoomsShared8017f108BuyDesc;

#endif // ROOMS_SHARED_8017F108_H
