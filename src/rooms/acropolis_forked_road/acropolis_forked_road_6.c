#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, installed on its entry task.
extern GpMsgEntry D_acropolis_forked_road_80180F14[];

/// First state of the room entry task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances the state.
void func_acropolis_forked_road_8017D92C(Task* task)
{
    task->msgTable = D_acropolis_forked_road_80180F14;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
