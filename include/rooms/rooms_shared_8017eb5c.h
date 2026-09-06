#ifndef ROOMS_SHARED_8017EB5C_H
#define ROOMS_SHARED_8017EB5C_H

#include "common.h"

#include "main/ui.h"

/// One row of the vending machine's mode list: the last row is the "cancel"
/// entry, the four above it fold their mode into the owner's `spawnArg1` and
/// grey themselves out when the room's id list has nothing to offer for that
/// mode. Confirming opens the item list. Seven rooms carry this body.
void RoomsShared8017eb5c(DialogPrompt* prompt, UiObject* obj);

/// Per-room data of the mode list: the four mode labels, the item-list panel
/// each row opens, and the room's own id-list lookup.
extern u8           RoomsShared8017eb5cMode0[];
extern u8           RoomsShared8017eb5cMode1[];
extern u8           RoomsShared8017eb5cMode2[];
extern u8           RoomsShared8017eb5cMode3[];
extern UiObjectDesc RoomsShared8017eb5cListDesc;

u16* RoomsShared8017eb5cIdList(s32 mode);

#endif // ROOMS_SHARED_8017EB5C_H
