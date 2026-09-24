#ifndef ROOMS_SHELTER_B1_ARMORY_H
#define ROOMS_SHELTER_B1_ARMORY_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/ui.h"
#include "rooms/room_common.h"

/// The item id the shop list's cursor last rested on.
extern s32 D_shelter_b1_armory_80182290;

/// Returns the 0xFFFF-terminated list of item ids the shop list starts from.
u16* func_shelter_b1_armory_8017D768(s32 mode);

/// Adds an item id to the shop list, one entry per item kind.
void func_shelter_b1_armory_8017E264(RoomShopList* shop, UiObject* obj, s32 item);

/// Fills the shop list from the id list its task's mode selects.
void func_shelter_b1_armory_8017E3B0(RoomShopList* shop, UiObject* obj);

/// Event gate for a door message: latches it and spawns the event task once
/// the request's prerequisites hold.
s32 func_shelter_b1_armory_8017FF40(RoomEventReq* req, RoomEventMsg* msg);

/// Draws a glowing strip between `arg0[0]` and `arg0[1]`.
void func_shelter_b1_armory_80180934(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);

/// Draws a glowing four-wedge disc at `arg0`.
void func_shelter_b1_armory_8018111C(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws a tinted, flickering sixteen-wedge disc at `arg0`.
void func_shelter_b1_armory_801814C0(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHELTER_B1_ARMORY_H
