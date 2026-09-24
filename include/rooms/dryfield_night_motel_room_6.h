#ifndef ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_6_H
#define ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_6_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/ui.h"

/// The "%" suffix appended to the percentages the play-data panels print.
extern u8 D_dryfield_night_motel_room_6_80182BD0[];

/// UI descriptor the play-data panels spawn when they first open.
extern UiObjectDesc D_dryfield_night_motel_room_6_80182DE8;

/// Exit callback of the room's save-prompt UI tasks: releases `Wip_UiHolder`
/// if the task still owns it, then frees the UI object and kills the task.
void func_dryfield_night_motel_room_6_8017F45C(Task* task);

/// Task descriptor of the held-object reflections the mirror spawns.
extern TaskDesc D_dryfield_night_motel_room_6_80182E74;

/// First state of the mirror task: sets up the player's reflection.
void func_dryfield_night_motel_room_6_8017F498(Task* task);

/// Second state of the mirror task: the per-frame update of the reflection.
void func_dryfield_night_motel_room_6_8017F64C(Task* task);

/// Draws a pulsing gouraud marker of two diamonds and two diagonals at the
/// projection of `pos`.
void func_dryfield_night_motel_room_6_80181CD8(SVECTOR* pos, s32 rate, s32 size);

/// Draws a pulsing gouraud glow disc at the projection of `pos`.
void func_dryfield_night_motel_room_6_80182158(SVECTOR* pos, s32 rate, s32 size);

#endif // ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_6_H
