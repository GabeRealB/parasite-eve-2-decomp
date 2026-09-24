#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

extern u8  D_801156F9;
extern s32 D_mist_parking_8018D830;
extern s8  D_mist_parking_8018DA28[];

/// State handlers of the task `func_mist_parking_80182898` runs.
extern TaskFuncTable3 D_mist_parking_8017D7DC;

/// Runs the handler for the task's state from a stack copy of
/// `D_mist_parking_8017D7DC`.
void func_mist_parking_80182898(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_parking_8017D7DC;
    sp.funcs[task->state](task);
}

void func_mist_parking_801828F0(Task* task)
{
    GameActor* actor;
    GpWorkObj* work;
    s32        idx;
    s32        flag;
    u16        tick;

    actor = (GameActor*)(gameGetPtrSlot(3))->work;
    if (D_801156F9 == 0) {
        idx = actor->field_438[1].nextSet - 0x2F;
        if ((idx > 0) && (idx < D_mist_parking_8018D830)) {
            flag = D_mist_parking_8018DA28[idx];
        } else {
            flag = 0;
        }
        if (task->state == 0) {
            if ((flag != 0) || (task->spawnArg1 != 0)) {
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
            work = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
            func_800B0928(gameGetPtrSlot(3), (Task*)work->field_0, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}
