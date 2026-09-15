#include "common.h"

#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

/// The room's own `GpMsgEntry[]` - the message table this task publishes.
extern GpMsgEntry D_neo_ark_altar_8017EF98[];
/// Spawn table for the follow-up task the altar's message task starts.
extern TaskDesc D_neo_ark_altar_8017F088[];

void func_neo_ark_altar_8017DC40(s32 arg0);

s32 func_neo_ark_altar_8017D908(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_3", func_neo_ark_altar_8017D910);

/// State 0 of the altar's message task: park the room's message table in
/// `Task::field_24`, publish the task in pointer slot 7, record the choice made
/// at the altar (game flag 0xD9) through the cutscene driver, then start the
/// follow-up task and advance to state 1.
void func_neo_ark_altar_8017D974(Task* task)
{
    task->field_24 = D_neo_ark_altar_8017EF98;
    Game_SetPtrSlot(task, 7);
    func_neo_ark_altar_8017DC40(GameFlag_GetNibble(0xD9) & 0xFF);
    Task_SpawnFromTable(D_neo_ark_altar_8017F088, 0, 0, 0);
    task->state = (s32)(task->state + 1);
}

void func_neo_ark_altar_8017D9E0(void)
{
}
