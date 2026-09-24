#ifndef ROOMS_DRYFIELD_TRAILER_COACH_H
#define ROOMS_DRYFIELD_TRAILER_COACH_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "main/ui.h"
#include "rooms/room_common.h"

/// The "%" suffix the room's percentage formatters append.
extern u8 D_dryfield_trailer_coach_801845F8[];

/// The item id the shop list's cursor last rested on.
extern s32 D_dryfield_trailer_coach_80183D44;

/// Task descriptor table the room's cutscene tasks spawn from: the room spawns
/// entry 0 with a cutscene record as its argument, and
/// `func_dryfield_trailer_coach_80181D88` spawns entry 1 for the scene.
extern TaskDesc D_dryfield_trailer_coach_80184F7C;

/// Returns the 0xFFFF-terminated list of item ids the shop list starts from.
u16* func_dryfield_trailer_coach_8017D7F4(s32 mode);

/// Adds an item id to the shop list, one entry per item kind.
void func_dryfield_trailer_coach_8017E2F0(RoomShopList* shop, UiObject* obj, s32 item);

/// Fills the shop list from the id list its task's mode selects.
void func_dryfield_trailer_coach_8017E43C(RoomShopList* shop, UiObject* obj);

/// Exit callback of a prompt task: releases `Wip_UiHolder` if the task holds
/// it, then frees the UI object and kills the task.
void func_dryfield_trailer_coach_80181D4C(Task* task);

/// Draws a pulsing light shaft at a point in a coordinate's space.
void func_dryfield_trailer_coach_801829A8(GsCOORDINATE2* coord, SVECTOR* pos, s32 speed, s32 halfExtent);

#endif // ROOMS_DRYFIELD_TRAILER_COACH_H
