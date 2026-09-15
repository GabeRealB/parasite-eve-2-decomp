#include "common.h"

#include "rooms/dryfield_water_tank.h"

/// Message 0x7DB handler: restarts the water-tank script, clearing the two
/// work-block counters and moving the task to the state the payload carries.
void func_dryfield_water_tank_8017E174(Task* task, s32 msgId, DwtMsg7DB* msg)
{
    DwtScriptWork* work;
    s32            state;

    work                = (DwtScriptWork*)task->idMap;
    work->field_4C      = 0;
    work->field_54      = 0;
    state               = msg->field_2;
    task->killCountdown = 0;
    task->state         = state;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017E194);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017E1B4);
