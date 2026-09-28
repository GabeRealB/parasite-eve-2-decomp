#ifndef ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
#define ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H

#include "common.h"

#include <psyq/libgte.h>
#include "rooms/room.h"
#include "rooms/room_common.h"

#include "main/task_types.h"
#include "main/ui_types.h"

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

/// The departure the departure task carries out.
extern RoomDeparture D_shelter_b1_underground_parking_8018D77C;

/// The cutscene task's descriptor table; entry 0 runs a scene record, entry 1
/// is the scene's sub-task.
extern TaskDesc D_shelter_b1_underground_parking_8018720C[];

/// The "%" suffix appended to the play-data percentages.
extern u8 D_shelter_b1_underground_parking_8018691C[];

/// Descriptor of the play-data panels' shared frame.
extern UiObjectDesc D_shelter_b1_underground_parking_80186B34;

/// The item id the shop list's cursor last rested on.
extern s32 D_shelter_b1_underground_parking_80186FB0;

/// Flag tested as zero / non-zero when drawing the room's view-dependent
/// markers: it selects 0x180 or 0x60 as the second argument of their draw
/// calls. Its meaning is unproven.
extern u16 D_shelter_b1_underground_parking_8018D78C;

#endif // ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
