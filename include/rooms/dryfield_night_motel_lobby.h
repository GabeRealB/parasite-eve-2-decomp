#ifndef ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
#define ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Work block the motel lobby's examine task keeps at `Task::idMap` (0x1C) --
/// that slot is *not* a `TaskIdMap` here. `func_dryfield_night_motel_lobby_80180E98`
/// allocates it with `Mem_Calloc(0xA, 0)`, and every examine state reaches it
/// with `(DnmlExamineWork*)task->idMap`.
///
/// `promptKind` is the display mode `func_dryfield_night_motel_lobby_80180FD8`
/// forwards to `func_800D4E78` as it re-spawns the action prompt over the
/// coordinates the prompt itself holds; `promptBusy` is the flag
/// `func_dryfield_night_motel_lobby_8018103C` raises on a confirm, right before
/// it starts cap slot 9. `field_6` and `field_7` are the two phase flags
/// `func_dryfield_night_motel_lobby_801802A8` tests to decide which of its two
/// cursor draws to run this frame.
typedef struct DnmlExamineWork {
    /* 0x00 */ byte pad_0[0x4];
    /* 0x04 */ s8   promptKind;
    /* 0x05 */ s8   promptBusy;
    /* 0x06 */ s8   field_6;
    /* 0x07 */ s8   field_7;
    /* 0x08 */ byte pad_8[0x2];
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

/// Draws the examine cursor over the room's hotspot table and, on a confirm,
/// raises the phase flag cap slot 9 waits on.
void func_dryfield_night_motel_lobby_801802A8(Task* task);

/// Message handler for the lobby's `arg2 == 3` event: on the first visit it
/// latches the visit flag and starts the scene, otherwise it fills in the cap
/// script and spawns the cutscene task.
s32 func_dryfield_night_motel_lobby_8017FB7C(s32 arg0, s32 arg1, s32 arg2);

#endif // ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
