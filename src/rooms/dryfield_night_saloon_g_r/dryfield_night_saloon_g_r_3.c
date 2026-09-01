#include "common.h"
#include "main/session.h"
#include "main/task.h"
extern TaskDesc D_dryfield_night_saloon_g_r_80185068;

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r_3", func_dryfield_night_saloon_g_r_8017E050);

extern u8 D_8007216D;

void func_dryfield_night_saloon_g_r_8017E0A8(u8 arg0)
{
    D_8007216D            = arg0;
    Game_Session->field_5 = arg0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r_3", func_dryfield_night_saloon_g_r_8017E0C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r_3", func_dryfield_night_saloon_g_r_8017E28C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r_3", func_dryfield_night_saloon_g_r_8017E564);

s32 func_dryfield_night_saloon_g_r_8017E698(void)
{
    Display_InitModeObj(&D_dryfield_night_saloon_g_r_80185068, 0, 0, 0);
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r_3", func_dryfield_night_saloon_g_r_8017E6C8);
