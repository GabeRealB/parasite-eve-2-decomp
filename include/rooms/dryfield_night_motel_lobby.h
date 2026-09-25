#ifndef ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
#define ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Work block the motel lobby's examine task keeps at `Task::work` (0x1C) --
/// that slot is *not* a `TaskIdMap` here. `func_dryfield_night_motel_lobby_80180E98`
/// allocates it with `memCalloc(0xA, 0)`, and every examine state reaches it
/// with `(DnmlExamineWork*)task->work`.
///
/// `promptKind` is the display mode `func_dryfield_night_motel_lobby_80180FD8`
/// forwards to `func_800D4E78` as it re-spawns the action prompt over the
/// coordinates the prompt itself holds; `promptBusy` is the flag
/// `func_dryfield_night_motel_lobby_8018103C` raises on a confirm, right before
/// it starts cap slot 9. `func_dryfield_night_motel_lobby_801802A8` reads
/// `field_6` and `field_7`: while `field_6` is clear it blanks all seven code
/// digits, and while `field_7` is clear it draws them. Once `field_7` is set,
/// it clears the most recent digit instead.
typedef struct DnmlExamineWork {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ s8  promptKind;
    /* 0x05 */ s8  promptBusy;
    /* 0x06 */ s8  field_6;
    /* 0x07 */ s8  field_7;
    /* 0x08 */ s8  field_8;
    /* 0x09 */ u8  pad_9;
} DnmlExamineWork;
STATIC_ASSERT_SIZEOF(DnmlExamineWork, 0xA);

/// The lobby's hotspot table: fifteen `OverlayHotspot` entries, the last of them
/// (index 14) the `id == -1` terminator the scans stop on. The room's init
/// clears every entry's `hit` flag on the way in.
extern OverlayHotspot D_dryfield_night_motel_lobby_80182820[];

/// The eleven states of the room's examine task, run by
/// `func_dryfield_night_motel_lobby_80180D58`.
extern const TaskFuncTable11 D_dryfield_night_motel_lobby_8017D6B0;

/// Draws the keypad's seven digit slots, or blanks them all to `0xA` while the
/// keypad is not yet in use; see `DnmlExamineWork`.
void func_dryfield_night_motel_lobby_801802A8(Task* task);

/// Applies one keypad press, `key` being the id of the hotspot pressed. Keys
/// 0-9 shift that digit in at index 0, 10 shifts in two zeros, 11 and 12 clear
/// the entry and 13 confirms it, raising `field_8` when the code checks out and
/// playing the reject sound otherwise. A digit is refused once seven are
/// entered, and a zero is refused while the entry is a lone zero.
void func_dryfield_night_motel_lobby_80180440(Task* task, s16 key);

/// Hit-tests (`x`, `y`) against every entry of the hotspot `table`, setting
/// each entry's `hit` flag, and returns whether any entry was hit.
s32 func_dryfield_night_motel_lobby_80180DE4(OverlayHotspot* table, s16 x, s16 y);

/// States of the examine task, in the order `D_dryfield_night_motel_lobby_8017D6B0`
/// lists them.
void func_dryfield_night_motel_lobby_80180E98(Task* task);
void func_dryfield_night_motel_lobby_80180FA4(Task* task);
void func_dryfield_night_motel_lobby_80180FD8(Task* task);
void func_dryfield_night_motel_lobby_8018103C(Task* task);
void func_dryfield_night_motel_lobby_801810AC(Task* task);
void func_dryfield_night_motel_lobby_80181138(Task* task);
void func_dryfield_night_motel_lobby_8018119C(Task* task);
void func_dryfield_night_motel_lobby_801811E0(Task* task);
void func_dryfield_night_motel_lobby_80181218(Task* task);
void func_dryfield_night_motel_lobby_8018122C(Task* task);

#endif // ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
