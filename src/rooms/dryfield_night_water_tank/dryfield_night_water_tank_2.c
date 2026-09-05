#include "common.h"

#include "gameplay/268.h"
#include "main/session.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_tank/dryfield_night_water_tank_2", func_dryfield_night_water_tank_8017D73C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_tank/dryfield_night_water_tank_2", func_dryfield_night_water_tank_8017D76C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_tank/dryfield_night_water_tank_2", func_dryfield_night_water_tank_8017D870);

void func_dryfield_night_water_tank_8017D94C(void)
{
    if (Game_Session->field_9 == 0xB) {
        Gp_MarkPlayTime();
    }
}
