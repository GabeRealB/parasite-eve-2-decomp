#ifndef ROOMS_SHARED_8017DE9C_H
#define ROOMS_SHARED_8017DE9C_H

#include "common.h"

#include "main/text.h"
#include "main/ui.h"

/// The literal "100.0%" a full share prints instead of a computed percentage.
/// Every carrying room has its own copy in its leading rodata, named at that
/// room's address by the family's symbol maps - the shared object reaches it
/// by name so it owns no `.rodata` of its own.
extern u8 RoomsShared8017de9cHundred[];

/// Draws one row of a "Play Data" usage panel: the item name, its share of the
/// recorded uses as a percentage, and a gauge whose length is the row's
/// `barWidths` fraction of the panel's inner width. The three parallel arrays
/// come from the `RoomItemUsage` block the panel's task parked in
/// `Task::idMap`; `RoomPeUsage` has the same layout, so the same row handler
/// serves the Parasite Energy panel.
///
/// A share of 10000 (a full 100%) prints as `RoomsShared8017de9cHundred`;
/// anything else is printed with `decimals = 2` and then shifted one place
/// right so the last two digits become the fraction. Highlighting the row
/// previews its item, and pressing the item-detail button on the selected row
/// opens `D_8010EFA0`.
void RoomsShared8017de9c(DialogPrompt* arg0, UiObject* arg1);

#endif // ROOMS_SHARED_8017DE9C_H
