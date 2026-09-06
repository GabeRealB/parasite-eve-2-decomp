#ifndef ROOMS_SHARED_8017F388_H
#define ROOMS_SHARED_8017F388_H

#include "common.h"

#include "main/text.h"
#include "main/ui.h"

/// The row's caption, "PE Data", and the descriptor the panel it opens is built
/// from. Each carrying room holds its own copy, so the shared object only names
/// them.
extern u8           RoomsShared8017f388Msg[];
extern UiObjectDesc RoomsShared8017f388Desc;

/// The save point's "PE Data" row: redraws its caption every frame and, once
/// the row is highlighted and the confirm button is down, opens the PE half of
/// the data panel and hands the menu back to its owner. Nineteen rooms carry
/// this body.
void RoomsShared8017f388(DialogPrompt* prompt, UiObject* obj);

#endif // ROOMS_SHARED_8017F388_H
