#include "common.h"

#include "main/session.h"
#include "main/task.h"

extern TaskDesc D_dryfield_night_saloon_g_r_80185068;

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r_4", func_dryfield_night_saloon_g_r_8017E564);

s32 func_dryfield_night_saloon_g_r_8017E698(void)
{
    Display_InitModeObj(&D_dryfield_night_saloon_g_r_80185068, 0, 0, 0);
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r_4", func_dryfield_night_saloon_g_r_8017E6C8);
