#ifndef ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
#define ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

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

void func_shelter_b1_underground_parking_80183B9C(void);

/// Lives in `shelter_b1_underground_parking_10`: maps the committed
/// selection `D_shelter_b1_underground_parking_8018D788 & 0xF` through the
/// room's destination table and writes the result into the session.
void func_shelter_b1_underground_parking_8018491C(void);

#endif // ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
