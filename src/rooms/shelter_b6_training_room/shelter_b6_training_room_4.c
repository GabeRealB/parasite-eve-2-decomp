#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

extern u8 D_801156F9;

void func_shelter_b6_training_room_8017D9C8(Task* task)
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
            work = Gp_FindWorkById(gGameSession->at4.loc.area | ((gGameSession->at4.loc.stage << 8) | 0x1000));
            func_800B0928(gameGetPtrSlot(3), (Task*)work->field_0, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}
