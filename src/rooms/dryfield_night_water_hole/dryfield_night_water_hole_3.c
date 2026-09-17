#include "common.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/rooms_shared_8017e690.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole_3", func_dryfield_night_water_hole_8017DE88);

void func_dryfield_night_water_hole_8017DF28(Task* task);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole_3", func_dryfield_night_water_hole_8017DF28);

void func_dryfield_night_water_hole_8017E630(Task* task)
{
    TaskFunc states[2] = { RoomsShared8017e690, func_dryfield_night_water_hole_8017DF28 };

    states[task->state](task);
    Game_Session->field_122 = -0x1A4;
}
