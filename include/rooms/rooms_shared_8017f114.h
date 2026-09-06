#ifndef ROOMS_SHARED_8017F114_H
#define ROOMS_SHARED_8017F114_H

#include "common.h"

#include "main/text.h"
#include "main/ui.h"

/// The prompt text this dialog draws, held by each carrying room.
extern u8 RoomsShared8017f114Msg[];

/// The save point's "save here?" confirmation: draws its prompt every frame and,
/// once the yes row is highlighted, the confirm button is down and the CD queue
/// is idle, opens the save panel and closes itself. Nineteen rooms carry this
/// body.
void RoomsShared8017f114(DialogPrompt* prompt, UiObject* obj);

#endif // ROOMS_SHARED_8017F114_H
