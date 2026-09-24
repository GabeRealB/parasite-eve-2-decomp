#ifndef ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_H
#define ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/ui.h"
#include "rooms/room_common.h"

/// The "%" suffix the room's percentage formatters append.
extern u8 D_dryfield_night_trailer_coach_80184D44[];

/// The item id the shop list's cursor last rested on.
extern s32 D_dryfield_night_trailer_coach_80184490;

/// Task descriptor table the room's cutscene tasks spawn from: the room spawns
/// entry 0 with a cutscene record as its argument, and
/// `func_dryfield_night_trailer_coach_80181DB0` spawns entry 1 for the scene.
extern TaskDesc D_dryfield_night_trailer_coach_80184FE4;

/// Returns the 0xFFFF-terminated list of item ids the shop list starts from.
u16* func_dryfield_night_trailer_coach_8017D81C(s32 mode);

/// Adds an item id to the shop list, one entry per item kind.
void func_dryfield_night_trailer_coach_8017E318(RoomShopList* shop, UiObject* obj, s32 item);

/// Fills the shop list from the id list its task's mode selects.
void func_dryfield_night_trailer_coach_8017E464(RoomShopList* shop, UiObject* obj);

/// Exit callback of a prompt task: releases `Wip_UiHolder` if the task holds
/// it, then frees the UI object and kills the task.
void func_dryfield_night_trailer_coach_80181D74(Task* task);

/// Glow markers the room's view handlers draw at world-space points.
void func_dryfield_night_trailer_coach_80182AB8(SVECTOR* pos, s32 speed, s32 size);
void func_dryfield_night_trailer_coach_80182F2C(SVECTOR* pos, s32 speed, s32 size);

#endif // ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_H
