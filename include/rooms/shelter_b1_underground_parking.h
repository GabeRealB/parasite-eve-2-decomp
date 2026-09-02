#ifndef ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
#define ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H

#include "common.h"

#include "main/task.h"

/// Gameplay-side action-prompt state, imported by this overlay as the unnamed
/// data symbol `D_80114D28` (the gameplay symbol map has no name for it yet).
///
/// `func_shelter_b1_underground_parking_80184468` scans the room's hotspot
/// table and, on a hit, stores the hotspot id in `targetId` and a mode in
/// `mode` (0 = nothing under the cursor, 1 = a hotspot is highlighted, 2 = the
/// hotspot is confirmed). `screenX` / `screenY` are the coordinates handed to
/// `func_800D4E78`, which parks them in the gameplay globals the prompt's
/// display task reads. Fields this overlay never touches are left as padding.
typedef struct SbupActionPrompt {
    /* 0x00 */ byte pad_0[0x8];
    /* 0x08 */ s16  screenX;
    /* 0x0A */ s16  screenY;
    /* 0x0C */ s16  targetId;
    /* 0x0E */ byte pad_E[0x2];
    /* 0x10 */ u8   mode;
    /* 0x11 */ byte pad_11[0x3];
    /* 0x14 */ u16  field_14;
    /* 0x16 */ byte pad_16[0x6];
    /* 0x1C */ u16  field_1C;
    /* 0x1E */ byte pad_1E[0x2];
} SbupActionPrompt;

/// Work block of the parking-lot examine task, hung off the `Task::idMap` slot
/// (0x1C) -- that slot is *not* a `TaskIdMap` here. Reach it with
/// `(SbupExamineWork*)task->idMap`.
///
/// `func_shelter_b1_underground_parking_80184468` copies a matched hotspot's
/// two table fields into `field_C` and `promptKind`;
/// `func_shelter_b1_underground_parking_80184594` forwards `promptKind` to
/// `func_800D4E78` as the display mode of the prompt it spawns.
typedef struct SbupExamineWork {
    /* 0x00 */ byte pad_0[0xC];
    /* 0x0C */ s16  field_C;
    /* 0x0E */ s8   promptKind;
    /* 0x0F */ byte pad_F[0x1];
} SbupExamineWork;

extern SbupActionPrompt D_80114D28;

void func_shelter_b1_underground_parking_80183B9C(void);

/// Lives in `shelter_b1_underground_parking_10`: maps the committed
/// selection `D_shelter_b1_underground_parking_8018D788 & 0xF` through the
/// room's destination table and writes the result into the session.
void func_shelter_b1_underground_parking_8018491C(void);

#endif // ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
