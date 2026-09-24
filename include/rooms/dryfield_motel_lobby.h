#ifndef ROOMS_DRYFIELD_MOTEL_LOBBY_H
#define ROOMS_DRYFIELD_MOTEL_LOBBY_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

/// The "%" suffix appended to the percentages the play-data panels print.
extern u8 D_dryfield_motel_lobby_8017F570[];

/// UI descriptor the "Play Data" panel and the usage panel spawn when they
/// first open.
extern UiObjectDesc D_dryfield_motel_lobby_8017F788;

/// Exit callback `func_dryfield_motel_lobby_8017ECE0` installs on its task:
/// clears `Wip_UiHolder` if the task's UI object still holds it, then frees
/// the object and kills the task.
void func_dryfield_motel_lobby_8017F3D0(Task* task);

#endif // ROOMS_DRYFIELD_MOTEL_LOBBY_H
