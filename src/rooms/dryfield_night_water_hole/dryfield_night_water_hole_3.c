#include "common.h"
#include "main/display.h"
#include "gameplay/gameplay.h"
extern s8 D_8007217B;

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole_3", func_dryfield_night_water_hole_8017DE30);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole_3", func_dryfield_night_water_hole_8017DE88);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole_3", func_dryfield_night_water_hole_8017DF28);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole_3", func_dryfield_night_water_hole_8017E630);

void func_dryfield_night_water_hole_8017E690(Task* arg0)
{
    if (D_8007217B == 0) {
        Game_Session->field_80 = 0;
    } else {
        Game_Session->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole_3", func_dryfield_night_water_hole_8017E6D0);
