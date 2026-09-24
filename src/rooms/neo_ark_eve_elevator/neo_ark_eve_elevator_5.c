#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, which state 0 of its event task installs.
extern GpMsgEntry D_neo_ark_eve_elevator_8017D724[];

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
void func_neo_ark_eve_elevator_8017D678(Task* task)
{
    task->msgTable = D_neo_ark_eve_elevator_8017D724;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
