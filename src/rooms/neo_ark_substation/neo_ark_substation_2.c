#include "common.h"

#include "main/gameflag.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

/// The room's own `GpMsgEntry[]` - the message table this task publishes.
extern GpMsgEntry D_neo_ark_substation_8017E294[];
/// Spawn table for the follow-up task this state starts.
extern TaskDesc D_neo_ark_substation_8017E2BC[];

s32 func_neo_ark_substation_8017D768(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0xDF) != 0 ? 3 : 5);
    }
    return 0;
}

s32 func_neo_ark_substation_8017D7A4(void)
{
    return 0;
}

/// State 0 of the neo_ark_substation message task: park the room's message
/// table in `Task::field_24`, publish the task in pointer slot 7, then - only
/// while game flag 0xDF is clear - start the follow-up task and advance to
/// state 1.
void func_neo_ark_substation_8017D7AC(Task* task)
{
    task->field_24 = D_neo_ark_substation_8017E294;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0xDF) == 0) {
        Task_SpawnFromTable(D_neo_ark_substation_8017E2BC, 0, 0, 0);
    }
    task->state = (s32)(task->state + 1);
}

void func_neo_ark_substation_8017D814(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_substation/neo_ark_substation_2", D_neo_ark_substation_8017D608);
