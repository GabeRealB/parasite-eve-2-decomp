#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, which its tasks answer from.
extern GpMsgEntry D_shelter_b1_main_corridor_801830A4[];

/// Installs the room's message table on `task`, registers the task in pointer
/// slot 7 and steps it to its next state.
void func_shelter_b1_main_corridor_8017DD4C(Task* task)
{
    task->msgTable = D_shelter_b1_main_corridor_801830A4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
