#ifndef ROOMS_ACROPOLIS_SECURITY_ROOM_H
#define ROOMS_ACROPOLIS_SECURITY_ROOM_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// 0xA work block of the security-monitor task, hung off the `Task::idMap`
/// slot (0x1C) -- that slot is *not* a `TaskIdMap` here, it is the
/// `Mem_Calloc(0xA)` block `func_acropolis_security_room_8017D9DC` allocates.
/// Reach it with `(AsrMonitorWork*)task->idMap`.
///
/// `cameraId` is the camera the monitor is currently showing, seeded from the
/// `GameFlag_GetNibble(0x2A)` lookup table and offset by 0x7F before being
/// handed to the panel drawer `func_acropolis_security_room_8017E0C4`.
/// `blinkTimer` is the cursor blink counter the overlay drawer
/// `func_acropolis_security_room_8017E37C` advances, `selection` the row the
/// player has highlighted, and `promptKind` the display mode forwarded to
/// `func_800D4E78` when the action prompt is spawned.
typedef struct AsrMonitorWork {
    /* 0x00 */ u16  cameraId;
    /* 0x02 */ s16  blinkTimer;
    /* 0x04 */ s16  selection;
    /* 0x06 */ s8   promptKind;
    /* 0x07 */ byte pad_7[0x1];
    /* 0x08 */ byte pad_8[0x2];
} AsrMonitorWork;
STATIC_ASSERT_SIZEOF(AsrMonitorWork, 0xA);

/// The security-monitor task's state table: seven handlers plus the NULL slot
/// the original `.rodata` table ends with. Only the seven handlers are copied
/// onto the stack by the dispatcher `func_acropolis_security_room_8017ED68`,
/// which is why the terminator is a separate member rather than an eighth
/// entry of `states`.
typedef struct AsrMonitorStateTable {
    /* 0x00 */ TaskFuncTable7 states;
    /* 0x1C */ TaskFunc       end;
} AsrMonitorStateTable;
STATIC_ASSERT_SIZEOF(AsrMonitorStateTable, 0x20);

/// Draws the security-monitor panel for the camera `id` (the work block's
/// `cameraId` biased by -0x7F); a negative `id` draws the "no signal" panel.
void func_acropolis_security_room_8017E0C4(s16 id);

/// Draws the blinking cursor overlay on top of the monitor panel.
void func_acropolis_security_room_8017E37C(Task* task);

#endif // ROOMS_ACROPOLIS_SECURITY_ROOM_H
