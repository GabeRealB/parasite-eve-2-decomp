#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/sound.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc D_dryfield_night_motel_balcony_80182834;

void func_dryfield_night_motel_balcony_8017E0C8(Task* arg0)
{
    Display_SpawnWithOt(&D_dryfield_night_motel_balcony_80182834, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    SndEvt_EnqueueType7(0x60010001, 1);
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8017E128);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8017E250);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8017E3C8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8017E4B8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8017E554);
