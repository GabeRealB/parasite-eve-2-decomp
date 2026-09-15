#include "common.h"

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "main/task.h"
#include "rooms/dryfield_night_motel_lobby.h"
#include "rooms/room_common.h"

/// Re-spawns the action prompt over the examine cursor: clears the highlight
/// state the prompt was left in, then hands the prompt's own coordinates and
/// this room's display mode back to `func_800D4E78`, which parks them in the
/// gameplay-side globals the prompt's display task reads.
void func_dryfield_night_motel_lobby_80180FD8(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    DnmlExamineWork*  work   = (DnmlExamineWork*)task->idMap;

    func_dryfield_night_motel_lobby_801802A8(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Confirms the action prompt the script's current step put up: drops the
/// highlight state, then, while `func_800D4EC0` still reports a prompt on
/// screen, flags the step busy in `promptBusy` (which the cursor draw in
/// `func_dryfield_night_motel_lobby_801802A8` gates its confirm on) and starts
/// cap slot 9. Advances the task to state 2 either way.
void func_dryfield_night_motel_lobby_8018103C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    DnmlExamineWork*  work   = (DnmlExamineWork*)task->idMap;

    func_dryfield_night_motel_lobby_801802A8(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        work->promptBusy = 1;
        Gp_StartCapSlot(9, 0, 0);
    }
    task->state = 2;
}
