#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// Message handlers the room's task installs in pointer slot 7.
extern GpMsgEntry D_shelter_b1_armory_80182500[];

/// Installs the room's message table in pointer slot 7 and advances the task.
void func_shelter_b1_armory_80180740(Task* task)
{
    task->msgTable = D_shelter_b1_armory_80182500;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
