#include "common.h"

#include "gameplay/3688.h"
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

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_7", func_dryfield_night_motel_lobby_8018103C);
