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

/// Draws the examine cursor over the room's hotspot table and, on a confirm,
/// raises the phase flag cap slot 9 waits on.
void func_dryfield_night_motel_lobby_801802A8(Task* task);

#endif // ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
