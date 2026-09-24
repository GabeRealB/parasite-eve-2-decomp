#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table: handlers for messages 0x13EE, 0x13F1, 0x13EF and
/// 0x13F0, closed by a 0x7FFFFFFF entry.
extern GpMsgEntry D_dryfield_night_warehouse_8017E830[];

/// State 0 of the room task: publishes the room's message table in
/// `Task::msgTable`, claims pointer slot 7 and advances to the next state.
void func_dryfield_night_warehouse_8017D610(Task* task)
{
    task->msgTable = D_dryfield_night_warehouse_8017E830;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
