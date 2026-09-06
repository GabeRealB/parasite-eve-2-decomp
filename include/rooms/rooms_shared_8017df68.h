#ifndef ROOMS_SHARED_8017DF68_H
#define ROOMS_SHARED_8017DF68_H

#include "common.h"

#include "main/text.h"
#include "main/ui.h"

/// Per-room data of the vending-machine row: the id the confirmed row wrote
/// back, the holder caption the map row shows, the map row's own label and the
/// map-panel descriptor, the "sold out" row label, and the buy-panel
/// descriptor.
extern s32          RoomsShared8017df68Selected;
extern u8           RoomsShared8017df68HolderText[];
extern u8           RoomsShared8017df68MapMsg[];
extern UiObjectDesc RoomsShared8017df68MapDesc;
extern u8           RoomsShared8017df68RowMsg[];
extern UiObjectDesc RoomsShared8017df68BuyDesc;

/// One row of a vending machine's item list: draws the row - a map entry, the
/// "sold out" placeholder or a real item with its price - keeps the holder
/// preview in step with the highlight, and on confirm opens the buy panel (or
/// the item detail panel on the second button). Seven rooms carry this body.
void RoomsShared8017df68(DialogPrompt* prompt, UiObject* obj);

#endif // ROOMS_SHARED_8017DF68_H
