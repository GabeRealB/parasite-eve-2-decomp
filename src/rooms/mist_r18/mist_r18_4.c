#include "common.h"

#include "main/display.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/mist_r18.h"

void RoomsShared8017df80(s32 shade);
void func_mist_r18_8017E144(s16 arg0);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_4", func_mist_r18_8017E854);

/// Fade the room in. `Task::killCountdown` is reused as the 0..0x80 fade level.
void func_mist_r18_8017E8B8(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown + 8;
    task->killCountdown = fade;
    if ((s16)fade >= 0x40) {
        task->killCountdown = 0x40;
    }
    if ((Game_Session->field_4D != 0) || ((u8)Game_Session->field_4 != 2)) {
        task->killCountdown = 0x80;
        task->state++;
    }
}

/// Fade the room back out; see `func_mist_r18_8017E8B8` for the level field.
void func_mist_r18_8017E92C(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown - 8;
    task->killCountdown = fade;
    if ((s16)fade <= 0) {
        task->killCountdown = 0;
        task->state++;
    }
    func_mist_r18_8017E144(task->killCountdown);
    RoomsShared8017df80(0x80 - task->killCountdown);
}
