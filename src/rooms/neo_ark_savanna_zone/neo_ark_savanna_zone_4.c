#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, which the room setup task installs.
extern GpMsgEntry D_neo_ark_savanna_zone_8017F9AC[];

/// State 0 of the room setup task: installs the room's message table and
/// pointer slot 7, then advances state.
void func_neo_ark_savanna_zone_8017D908(Task* task)
{
    task->msgTable = D_neo_ark_savanna_zone_8017F9AC;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
