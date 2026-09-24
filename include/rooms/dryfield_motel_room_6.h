#ifndef ROOMS_DRYFIELD_MOTEL_ROOM_6_H
#define ROOMS_DRYFIELD_MOTEL_ROOM_6_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/ui.h"

/// The "%" suffix appended to the percentages the play-data panels print.
extern u8 D_dryfield_motel_room_6_80182A68[];

/// UI descriptor the play-data panels spawn when they first open.
extern UiObjectDesc D_dryfield_motel_room_6_80182C80;

/// Exit callback of the room's save-prompt UI tasks: releases `Wip_UiHolder`
/// if the task still owns it, then frees the UI object and kills the task.
void func_dryfield_motel_room_6_8017F440(Task* task);

/// Task descriptor of the held-object reflections the mirror spawns.
extern TaskDesc D_dryfield_motel_room_6_80182D0C;

/// First state of the mirror task: sets up the player's reflection.
void func_dryfield_motel_room_6_8017F47C(Task* task);

/// Second state of the mirror task: the per-frame update of the reflection.
void func_dryfield_motel_room_6_8017F630(Task* task);

/// Fallback of the room's message-0x13F0 handler for every event other than
/// the cutscene's; this room does nothing with them.
void func_dryfield_motel_room_6_80181910(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

/// Draws a pulsing gouraud marker of two diamonds and two diagonals at the
/// projection of `pos`.
void func_dryfield_motel_room_6_80181B70(SVECTOR* pos, s32 rate, s32 size);

/// Draws a pulsing gouraud glow disc at the projection of `pos`.
void func_dryfield_motel_room_6_80181FF0(SVECTOR* pos, s32 rate, s32 size);

#endif // ROOMS_DRYFIELD_MOTEL_ROOM_6_H
