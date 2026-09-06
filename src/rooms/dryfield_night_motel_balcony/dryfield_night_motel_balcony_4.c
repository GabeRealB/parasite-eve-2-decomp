#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/wipsys.h"
#include "psyq/libgpu.h"

extern s8       D_8007106B;
extern TaskDesc D_dryfield_night_motel_balcony_80182834;

extern void func_dryfield_night_motel_balcony_8017E250(s32 arg0, s16 arg1);

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

void func_dryfield_night_motel_balcony_8017E3C8(void)
{
    func_dryfield_night_motel_balcony_8017E250(0, GameFlag_GetNibble(0x85));
    func_dryfield_night_motel_balcony_8017E250(1, GameFlag_GetNibble(0x86));
    func_dryfield_night_motel_balcony_8017E250(2, GameFlag_GetNibble(0x87));
    func_dryfield_night_motel_balcony_8017E250(3, GameFlag_GetNibble(0x88));
    func_dryfield_night_motel_balcony_8017E250(4, GameFlag_GetNibble(0x89));
    func_dryfield_night_motel_balcony_8017E250(5, GameFlag_GetNibble(0x8A));
    func_dryfield_night_motel_balcony_8017E250(6, GameFlag_GetNibble(0x8B));
    func_dryfield_night_motel_balcony_8017E250(7, GameFlag_GetNibble(0x8C));
    func_dryfield_night_motel_balcony_8017E250(8, GameFlag_GetNibble(0x8D));
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8017E4B8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8017E554);
