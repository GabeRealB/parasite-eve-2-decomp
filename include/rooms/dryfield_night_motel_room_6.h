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

/// First state of the mirror task: sets up the player's reflection.
void func_dryfield_night_motel_room_6_8017F498(Task* task);

/// Second state of the mirror task, run every frame after the set-up.
void func_dryfield_night_motel_room_6_8017F64C(Task* task);

#endif // ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_6_H
