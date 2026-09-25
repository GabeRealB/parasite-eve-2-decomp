#ifndef ROOMS_MIST_PARKING_H
#define ROOMS_MIST_PARKING_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/3CD8.h"
#include "main/task.h"
#include "main/ui.h"
#include "rooms/room_common.h"

/// Task descriptor tables the room spawns its tasks from.
extern TaskDesc D_mist_parking_801869B8;
extern TaskDesc D_mist_parking_8018D75C;
extern TaskDesc D_mist_parking_8018FC24;
extern TaskDesc D_mist_parking_80190824;

/// Tasks the room keeps a handle on while they run.
extern Task* D_mist_parking_80195318;
extern Task* D_mist_parking_80195320;
extern Task* D_mist_parking_80195324;
extern Task* D_mist_parking_8019532C;

/// State handlers of the text-block task `func_mist_parking_801832AC` runs.
extern const TaskFuncTable3 D_mist_parking_8017D7F4;

/// The "%" suffix appended to the play-data percentages.
extern u8 D_mist_parking_80186718[];

/// The item id the shop list's cursor last rested on.
extern s32 D_mist_parking_8018644C;

/// Returns the 0xFFFF-terminated list of item ids the shop list starts from.
u16* func_mist_parking_8017D8F8(s32 mode);

/// Adds an item id to the shop list, one entry per item kind.
void func_mist_parking_8017E3F4(RoomShopList* shop, UiObject* obj, s32 item);

/// Fills the shop list from the id list its task's mode selects.
void func_mist_parking_8017E540(RoomShopList* shop, UiObject* obj);

/// Fill the play-data panels' item and PE usage rows.
void func_mist_parking_80180C98(UiList* list, UiObject* obj);
void func_mist_parking_80180F94(UiList* list, UiObject* obj);

/// Exit callback of a prompt task: releases `Wip_UiHolder` if the task holds
/// it, then frees the UI object and kills the task.
void func_mist_parking_80181E50(Task* task);

/// Attaches the task's model to a part of its parent's model.
void func_mist_parking_8018307C(Task* task);

/// Empty per-frame state handler.
void func_mist_parking_801830F8(Task* task);

/// The text-block task's set-up and wait states.
void func_mist_parking_80183304(Task* task);
void func_mist_parking_801833F8(Task* task);

/// Exit callback of the text-block task: kills it and calls
/// `Stage_SetEndingFlag`.
void func_mist_parking_80183434(Task* task);

/// Resets the caption state and, for 1 or 2, loads that caption file.
void func_mist_parking_80183708(s32 arg0);

/// Drop the handles of room tasks without killing them; the argument their
/// caller passes is unused.
void func_mist_parking_801837A4(s32 arg0);
void func_mist_parking_8018471C(s32 arg0);

/// Copies a placement onto the task's model coordinate frame.
s32 func_mist_parking_80183AC4(Task* task, s32 arg1, RoomPlacement* placement, s32 arg3);

/// Glow markers the room's view handler draws at world-space points.
void func_mist_parking_80184A18(SVECTOR* pos, s32 speed, s32 size);
void func_mist_parking_80184E8C(SVECTOR* pos, s32 speed, s32 size);
void func_mist_parking_80185814(SVECTOR* pos, s32 arg1, s32 color);

#endif // ROOMS_MIST_PARKING_H
