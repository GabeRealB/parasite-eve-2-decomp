#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, which its CAP scripts index.
extern GpMsgEntry D_shelter_b3_elevator_hall_80182A38[];

/// First state of the room's message-driven task: points the task at the
/// room's message table, publishes it in pointer slot 7 and advances.
void func_shelter_b3_elevator_hall_8017DDCC(Task* task)
{
    task->msgTable = D_shelter_b3_elevator_hall_80182A38;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
