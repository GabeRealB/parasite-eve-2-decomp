#include "common.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

void func_mine_cavern_8017E18C(Task* task)
{
    task->killCountdown++;
    switch (task->killCountdown) {
        case 0x21:
        case 0x6:
        case 0x40:
        case 0x7C:
        case 0x60:
        case 0x8C:
            SndEvt_EnqueueType6(0x1000003A, 0, 0x30);
            break;
        case 0x50:
        case 0x12:
        case 0x30:
        case 0x70:
        case 0x87:
        case 0x218:
            SndEvt_EnqueueType6(0x10000039, 0, 0x30);
            break;
    }
    if ((Game_Session->field_5F != 0) || (task->killCountdown >= 0x219)) {
        Task_Kill(task);
    }
}

void func_mine_cavern_8017E2D8(void)
{
    SndEvt_EnqueueType2(0, 0x64);
}
