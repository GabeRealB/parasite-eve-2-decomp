#include "common.h"

#include "main/session.h"
#include "main/task.h"

void func_mist_r18_8017DF80(s32 arg0);
void func_mist_r18_8017E144(s16 arg0);

/// Spawn descriptor handed to entry 5 of `D_mist_r18_80184F04`.
extern s32      D_mist_r18_80184EE4;
extern TaskDesc D_mist_r18_80184F04;

void func_mist_r18_8017E39C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E3A4);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E448);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E534);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E654);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E6D8);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E784);

void func_mist_r18_8017E7F0(void)
{
    Task_SpawnFromTable(&D_mist_r18_80184F04, 5, 0, (s32)&D_mist_r18_80184EE4);
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E824);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E854);

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
    func_mist_r18_8017DF80(0x80 - task->killCountdown);
}
