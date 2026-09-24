#ifndef ROOMS_SHELTER_1F_HELIPORT_H
#define ROOMS_SHELTER_1F_HELIPORT_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/ui.h"
#include "rooms/room_common.h"

/// The item id the shop list's cursor last rested on.
extern s32 D_shelter_1f_heliport_80180F48;

/// Returns the 0xFFFF-terminated list of item ids the shop list starts from.
u16* func_shelter_1f_heliport_8017D730(s32 mode);

/// Fills the shop list from the id list its task's mode selects.
void func_shelter_1f_heliport_8017E378(RoomShopList* shop, UiObject* obj);

#endif // ROOMS_SHELTER_1F_HELIPORT_H
