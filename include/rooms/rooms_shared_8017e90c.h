#ifndef ROOMS_SHARED_8017E90C_H
#define ROOMS_SHARED_8017E90C_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

#include "rooms/room_common.h"

/// The vending machine's item-list task: allocates the room's `RoomShopList`
/// into `Task::idMap` on the first frame, has the room fill it, lays it out
/// with the balance panel above, then draws the rule and the BP figure every
/// frame and services its children. Seven rooms carry this body.
void RoomsShared8017e90c(Task* task);

/// Per-room data of the item list: its caption, its row-callback table, the
/// balance panel it spawns and the room's own list builder.
extern u8             RoomsShared8017e90cTitle[];
extern UiListItemFunc RoomsShared8017e90cRowFuncs[];
extern UiObjectDesc   RoomsShared8017e90cBalanceDesc;

void RoomsShared8017e90cFillList(RoomShopList* shop, UiObject* obj);

#endif // ROOMS_SHARED_8017E90C_H
