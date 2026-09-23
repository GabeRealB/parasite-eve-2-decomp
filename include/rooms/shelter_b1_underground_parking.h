#ifndef ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
#define ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Work block of the parking-lot examine task, hung off the `Task::work` slot
/// (0x1C) -- that slot is *not* a `TaskIdMap` here. Reach it with
/// `(SbupExamineWork*)task->work`.
///
/// `func_shelter_b1_underground_parking_80184468` copies a matched hotspot's
/// two table fields into `field_C` and `promptKind`;
/// `func_shelter_b1_underground_parking_80184594` forwards `promptKind` to
/// `func_800D4E78` as the display mode of the prompt it spawns.
/// `fadeLevel` is the intensity of the closing fade: the last state raises it
/// each frame, clamps it at 0xFF and draws it on all three channels of
/// `Fade_DrawOverlay`.
typedef struct SbupExamineWork {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x4];
    /* 0x08 */ s16  fadeLevel;
    /* 0x0A */ byte pad_A[0x2];
    /* 0x0C */ s16  field_C;
    /* 0x0E */ s8   promptKind;
    /* 0x0F */ byte pad_F[0x1];
} SbupExamineWork;

void func_shelter_b1_underground_parking_8018390C(void);

void func_shelter_b1_underground_parking_80183B9C(void);

void func_shelter_b1_underground_parking_801848A4(void);

void func_shelter_b1_underground_parking_801857E0(s16 x, s16 y, s32 arg2, s32 arg3);

void func_shelter_b1_underground_parking_80186890(s16 arg0);

/// Looks up the low nibble of `D_shelter_b1_underground_parking_8018D788` in
/// the byte table `D_shelter_b1_underground_parking_801876C4`, stores the
/// result as the current room (both `D_8007216D` and the session's
/// `at4.loc.room`) and flags the room objects for relinking.
void func_shelter_b1_underground_parking_8018491C(void);

#endif // ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
