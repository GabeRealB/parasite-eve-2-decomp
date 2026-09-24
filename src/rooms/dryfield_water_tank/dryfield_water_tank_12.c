#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_water_tank.h"

/// Script command of the room's cutscene: stores `arg0` as the command the
/// cutscene task carries out next (`DwtWork::field_4`) and restarts its step
/// counter `field_6`. The block is reached through the cutscene task parked in
/// `D_dryfield_water_tank_80188D50`.
void func_dryfield_water_tank_8017EB80(s16 arg0)
{
    DwtWork* work = (DwtWork*)D_dryfield_water_tank_80188D50->work;

    work->field_4 = arg0;
    work->field_6 = 0;
}
