#ifndef ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
#define ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/ui.h"
#include "rooms/room.h"
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

/// Exit callback of the save prompt: releases `Wip_UiHolder` if the task holds
/// it, then frees the UI object and kills the task.
void func_shelter_b1_underground_parking_8017F7D0(Task* task);

/// Returns the 0xFFFF-terminated list of item ids the shop list starts from.
u16* func_shelter_b1_underground_parking_8017F80C(s32 mode);

/// Resets both action-prompt slots and steps the caller on one state.
void func_shelter_b1_underground_parking_801848BC(Task* task);

/// Marks every hotspot of `table` under (`x`, `y`) as hit; answers whether any
/// was.
s32 func_shelter_b1_underground_parking_80184964(OverlayHotspot* table, s16 x, s16 y);

/// Glow markers the room's view handler draws at world-space points.
void func_shelter_b1_underground_parking_80184C54(SVECTOR* pos, s32 size, s32 angle, s32 tint);
void func_shelter_b1_underground_parking_8018543C(SVECTOR* pos, s32 size, s32 tint);
void func_shelter_b1_underground_parking_80185A94(SVECTOR* pos, s32 speed, s32 size);
void func_shelter_b1_underground_parking_80185F08(SVECTOR* pos, s32 speed, s32 size);

/// Flag tested as zero / non-zero when drawing the room's view-dependent
/// markers: it selects 0x180 or 0x60 as the second argument of their draw
/// calls. Its meaning is unproven.
extern u16 D_shelter_b1_underground_parking_8018D78C;

void func_shelter_b1_underground_parking_8018390C(void);

void func_shelter_b1_underground_parking_801848A4(void);

void func_shelter_b1_underground_parking_801857E0(s16 x, s16 y, s16 radius, s16 color);

void func_shelter_b1_underground_parking_80186890(s16 arg0);

/// Looks up the low nibble of `D_shelter_b1_underground_parking_8018D788` in
/// the byte table `D_shelter_b1_underground_parking_801876C4`, stores the
/// result as the current room (both `Mc_SaveData.at4.loc.room` and the session's
/// `at4.loc.room`) and flags the room objects for relinking.
void func_shelter_b1_underground_parking_8018491C(void);

#endif // ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
