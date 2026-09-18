#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

extern Task* RoomsShared8017e320Task;

extern u8 D_801156F9;

void func_dryfield_main_street_8017E354(s32 arg0)
{
    Task* t = RoomsShared8017e320Task;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    Task_Kill(RoomsShared8017e320Task);
    RoomsShared8017e320Task = NULL;
}

void func_dryfield_main_street_8017E3A8(Task* task)
{
    GpWorkObj* work;
    u16        tick;

    if (D_801156F9 == 0) {
        if (task->state == 0) {
            if (task->spawnArg1 != 0) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            work = Gp_FindWorkById(gGameSession->loc.area | (gGameSession->loc.stage << 8));
            func_800B0928(Game_GetPtrSlot(3), (Task*)work->field_0, 0x300, 0x200, task->killCountdown);
        } else {
            Task_Kill(task);
        }
    }
}

void func_dryfield_main_street_8017E4A4(void)
{
    RoomsShared8017e320Task = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_5", func_dryfield_main_street_8017E4B0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_5", func_dryfield_main_street_8017E830);
