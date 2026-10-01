#include "gameplay/captions.h"

#include "types.h"

#include "captions.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

/// Object stored in `Task::spawnArg2` for `Gp_EndWaitTask`. `field_2` is a
/// signed completion flag: when non-zero the task calls `Stage_SetEndingFlag`
/// and kills itself.
typedef struct _GpEndWait {
    /* 0x00 */ byte pad_0[2];
    /* 0x02 */ s8   field_2;
} GpEndWait;

void func_800E70AC(Task* task)
{
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                if (D_80115666 == 2) {
                    Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_ROOM_EFFECT), 0xBB8, 1, 0);
                }
                task->state++;
                break;
        }
        func_800E44A0(task);
    }
}

void Gp_EndWaitTask(Task* task)
{
    GpEndWait* flag;

    flag = task->spawnArg2.pointer;
    switch (task->state) {
        case 0:
            Task_Spawn(1, 0x2C, 0, flag);
            task->state++;
            break;
        case 1:
            if (flag->field_2 != 0) {
                Stage_SetEndingFlag();
                taskKill(task);
            }
            break;
    }
}
