#ifndef ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
#define ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Work block the motel lobby's examine task keeps at `Task::work` (0x1C) --
/// that slot is *not* a `TaskIdMap` here. `func_dryfield_night_motel_lobby_80180E98`
/// allocates it with `Mem_Calloc(0xA, 0)`, and every examine state reaches it
/// with `(DnmlExamineWork*)task->work`.
///
/// `promptKind` is the display mode `func_dryfield_night_motel_lobby_80180FD8`
/// forwards to `func_800D4E78` as it re-spawns the action prompt over the
/// coordinates the prompt itself holds; `promptBusy` is the flag
/// `func_dryfield_night_motel_lobby_8018103C` raises on a confirm, right before
/// it starts cap slot 9. `field_6` and `field_7` are the two phase flags
/// `func_dryfield_night_motel_lobby_801802A8` tests to decide which of its two
/// cursor draws to run this frame.
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

/// `Task::spawnArg2` of the cap (cutscene) task this room family spawns, the
/// third argument the message handler receives as `arg2`.
///
/// `field_0` is the area id forced for the duration of the scene; `field_1`
/// selects the cap slot / command and `field_2` skips straight to the abort
/// state. `field_3` is the cap file to load: the motel lobby picks between two
/// takes of the same scene on `GameFlag_GetNibble(0x7A)`, one per side of the
/// count of times the player has been through it. The four s32s are
/// sound-event ids and `field_14` / `field_16` are the `func_800E6D4C` fade
/// pair, the same layout `Shelter1fTentCapScript` uses.
typedef struct DnmlCapScript {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} DnmlCapScript;

STATIC_ASSERT_SIZEOF(DnmlCapScript, 0x18);

extern DnmlCapScript D_dryfield_night_motel_lobby_801844E0;

/// The lobby's hotspot table: fifteen `RoomHotspot` entries, the last of them
/// (index 14) the `id == -1` terminator the scans stop on. The room's init
/// clears every entry's `hit` flag on the way in.
extern RoomHotspot D_dryfield_night_motel_lobby_80182820[];

/// The seven digits of the lobby keypad code as entered so far, most recent
/// first at index 0; `0xA` marks a slot the player has not filled. The room's
/// init resets all seven to `0xA`,
/// `func_dryfield_night_motel_lobby_80180440` shifts a new digit in at index 0
/// (its own count of digits entered is bounded by 7) and
/// `func_dryfield_night_motel_lobby_80180734` tests the filled slots against
/// the code. The datum's eighth byte is padding before the cap script.
extern u8 D_dryfield_night_motel_lobby_801844D8[7];

/// The lobby's eleven-entry task state table, in the room's leading rodata.
/// The dispatcher below copies it onto the stack, so the handler runs from the
/// copy rather than from here.
///
/// The same body carries the same-shaped table in `shelter_r47` (eleven
/// handlers at `0x8017D7DC`) and in `aya/replay_bonus`, which is why the three
/// copies are one body in the duplicate index. They cannot share an object as
/// things stand: the shared unit would have to reach each room's own table, and
/// `tools/overlay_dup_index.py promote` refuses exactly that (`RoomsShared8017d878`
/// shows the manual per-carrier name this would need).
extern const TaskFuncTable11 D_dryfield_night_motel_lobby_8017D6B0;

/// Dispatches the room's main task through the table above: the eleven handlers
/// are copied onto the stack and the one named by `Task::state` is called.
void func_dryfield_night_motel_lobby_80180D58(Task* task);

/// Draws the examine cursor over the room's hotspot table and, on a confirm,
/// raises the phase flag cap slot 9 waits on.
void func_dryfield_night_motel_lobby_801802A8(Task* task);

/// Whether the keypad above holds the lobby's code: exactly four digits, the
/// three older slots still `0xA`, and those four reading `3 0 3 3` in the
/// order they were typed. `func_dryfield_night_motel_lobby_80180440` calls it
/// on a confirm and sets its accept flag on a non-zero result.
s32 func_dryfield_night_motel_lobby_80180734(void);

/// Task callback of the descriptor at `D_dryfield_night_motel_lobby_80182814`:
/// allocates the examine work at `Task::work`, spawns the examine child task,
/// bumps the state once and resets the room's per-visit state -- the hotspot
/// hits, the keypad digits and three session flags.
void func_dryfield_night_motel_lobby_80180E98(Task* task);

/// Message handler for the lobby's `arg2 == 3` event: on the first visit it
/// latches the visit flag and starts the scene, otherwise it fills in the cap
/// script and spawns the cutscene task.
s32 func_dryfield_night_motel_lobby_8017FB7C(s32 arg0, s32 arg1, s32 arg2);

#endif // ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
