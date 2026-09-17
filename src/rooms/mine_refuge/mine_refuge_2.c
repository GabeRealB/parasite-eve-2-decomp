#include "common.h"

#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

extern Task*    D_mine_refuge_80182AD8;
extern TaskDesc D_801358D8;

void func_mine_refuge_8017FA08(Task* task)
{
    s32 sp10;

    switch (task->state) {
        case 0:
            if (GameFlag_GetNibble(0x166) == 1) {
                GameFlag_SetNibble(0x166, 2);
                Gp_RunCapCmd1(0xF);
            }
            task->state = task->state + 1;
            return;
        case 2:
            SndEvt_EnqueueType6(0x54060007, 0, 0);
            D_mine_refuge_80182AD8 = Task_SpawnFromTable(&D_801358D8, 0, 0, 0);
            task->state            = task->state + 1;
            return;
        case 3:
            if (Task_PollKill(D_mine_refuge_80182AD8, &sp10) != 0) {
                D_mine_refuge_80182AD8 = NULL;
                task->state            = task->state + 1;
            }
            return;
        case 1:
        case 4:
            task->state = task->state + 1;
            return;
        case 5:
            SndEvt_EnqueueType6(0x54060008, 0, 0);
            Task_Kill(task);
            break;
    }
}
