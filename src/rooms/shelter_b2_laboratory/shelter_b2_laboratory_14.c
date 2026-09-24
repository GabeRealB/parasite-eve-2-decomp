#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// Message table of the room's slot-7 task.
extern GpMsgEntry D_shelter_b2_laboratory_80182A38[];

/// Installs `D_shelter_b2_laboratory_80182A38` as the task's message table,
/// registers the task in pointer slot 7 and steps it on one state.
void func_shelter_b2_laboratory_80180450(Task* task)
{
    task->msgTable = D_shelter_b2_laboratory_80182A38;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
