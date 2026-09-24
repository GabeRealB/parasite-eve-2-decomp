#ifndef ROOMS_DRYFIELD_NIGHT_GARAGE_H
#define ROOMS_DRYFIELD_NIGHT_GARAGE_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/ui.h"
#include "rooms/room_common.h"

/// The item id the shop list's cursor last rested on.
extern s32 D_dryfield_night_garage_801819EC;

/// Returns the 0xFFFF-terminated list of item ids the shop list starts from.
u16* func_dryfield_night_garage_8017D754(s32 mode);

/// Adds an item id to the shop list, one entry per item kind.
void func_dryfield_night_garage_8017E250(RoomShopList* shop, UiObject* obj, s32 item);

/// Fills the shop list from the id list its task's mode selects.
void func_dryfield_night_garage_8017E39C(RoomShopList* shop, UiObject* obj);

/// Draws a glowing strip between `arg0[0]` and `arg0[1]`.
void func_dryfield_night_garage_80180D9C(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_DRYFIELD_NIGHT_GARAGE_H
