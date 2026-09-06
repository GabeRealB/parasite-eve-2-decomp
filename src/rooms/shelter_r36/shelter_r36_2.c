#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_r36_8017E97C[];
extern TaskDesc   D_shelter_r36_8017DF14;

s32 func_shelter_r36_8017D914(void)
{
    return 0;
}

s32 func_shelter_r36_8017D91C(void)
{
    return 0;
}

void func_shelter_r36_8017D924(Task* task)
{
    task->field_24 = D_shelter_r36_8017E97C;
    Game_SetPtrSlot(task, 7);
    if (Game_Session->field_8 == 1) {
        Task_SpawnFromTable(&D_shelter_r36_8017DF14, 0, 0, 0);
    }
    if (Game_Session->field_8 == 2) {
        Task_SpawnFromTable(&D_shelter_r36_8017DF14, 1, 0, 0);
    }
    task->state++;
}
