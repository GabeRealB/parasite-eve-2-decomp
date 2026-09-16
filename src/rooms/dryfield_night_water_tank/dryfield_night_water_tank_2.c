#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/session.h"

s32 func_dryfield_night_water_tank_8017D73C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xE) {
        Gp_StartCapSlot(0xE, 1, 1);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_tank/dryfield_night_water_tank_2", func_dryfield_night_water_tank_8017D76C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_tank/dryfield_night_water_tank_2", func_dryfield_night_water_tank_8017D870);

void func_dryfield_night_water_tank_8017D94C(void)
{
    if (Game_Session->field_9 == 0xB) {
        Gp_MarkPlayTime();
    }
}
