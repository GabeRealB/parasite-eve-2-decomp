#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern GpCapCmd** Gp_CapCmds;
extern TaskDesc   D_shelter_1f_guardroom_8017DA6C;
extern Task*      D_shelter_1f_guardroom_8017E014;

void func_shelter_1f_guardroom_8017D9CC(s32 arg0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_guardroom/shelter_1f_guardroom", D_shelter_1f_guardroom_8017D5C4);

void func_shelter_1f_guardroom_8017D5E8(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(2);
            goto next;
        case 1:
            if (Gp_CapBusy() == 0) {
                goto next;
            }
            break;
        case 2:
            Gp_CapCmds[2]->field_4 = 1;
            if (Gp_GetCapEventKey() != 0xB) {
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                break;
            }
            gGameSession->hideHud           = 1;
            D_shelter_1f_guardroom_8017E014 = Task_SpawnFromTable(&D_shelter_1f_guardroom_8017DA6C, 0, 0, 0);
            task->state++;
            break;
        case 3:
            if (Task_PollKill(D_shelter_1f_guardroom_8017E014, &poll) == 0) {
                break;
            }
        next:
            task->state++;
            break;
        case 4:
            gGameSession->hideHud = 0;
            func_shelter_1f_guardroom_8017D9CC(1);
            GameFlag_SetNibble(0xB2, 1);
            Gp_MsgPlayerWeapon(1);
            taskKill(task);
            break;
    }
}

s32 func_shelter_1f_guardroom_8017D73C(void)
{
    return 0;
}
