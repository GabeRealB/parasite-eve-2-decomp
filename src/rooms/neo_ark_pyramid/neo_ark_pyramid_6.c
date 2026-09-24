#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table: handlers for messages 0x13EE, 0x13F1, 0x13EF
/// and 0x13F0, closed by a 0x7FFFFFFF entry.
extern GpMsgEntry D_neo_ark_pyramid_8017FBE4[];

/// State 0 of the room's entry task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances.
void func_neo_ark_pyramid_8017DB18(Task* task)
{
    task->msgTable = D_neo_ark_pyramid_8017FBE4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
